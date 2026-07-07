#include "api/server.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <format>
#include <print>
#include <ranges>
#include <span>

namespace api {

// ═══════════════════════════════════════════════════════════
// Database implementation (RAII wrapper over sqlite3)
// ═══════════════════════════════════════════════════════════
struct Database::Impl {
  sqlite3* db = nullptr;
};

Database::Database(std::string_view path) : impl_(std::make_unique<Impl>()) {
  int rc = sqlite3_open(path.data(), &impl_->db);
  if (rc != SQLITE_OK) {
    auto msg = std::format("Failed to open database: {}", sqlite3_errmsg(impl_->db));
    sqlite3_close(impl_->db);
    throw std::runtime_error(msg);
  }

  // Enable WAL mode for better concurrency
  execute("PRAGMA journal_mode=WAL");
  // Create items table
  execute(R"(
    CREATE TABLE IF NOT EXISTS items (
      id          INTEGER PRIMARY KEY AUTOINCREMENT,
      name        TEXT    NOT NULL,
      description TEXT    DEFAULT '',
      created_at  TEXT    DEFAULT (datetime('now')),
      updated_at  TEXT    DEFAULT (datetime('now'))
    )
  )");

  spdlog::info("Database initialized at {}", path);
}

Database::~Database() {
  if (impl_ && impl_->db) {
    sqlite3_close(impl_->db);
    spdlog::debug("Database closed");
  }
}

Database::Database(Database&&) noexcept = default;
Database& Database::operator=(Database&&) noexcept = default;

Result<std::vector<json>> Database::query(const std::string& sql) {
  sqlite3_stmt* stmt = nullptr;
  if (int rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr);
      rc != SQLITE_OK) {
    return std::unexpected(Error{500, sqlite3_errmsg(impl_->db)});
  }

  std::vector<json> rows;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    json row;
    int col_count = sqlite3_column_count(stmt);
    for (int i = 0; i < col_count; ++i) {
      std::string col_name = sqlite3_column_name(stmt, i);
      switch (sqlite3_column_type(stmt, i)) {
        case SQLITE_INTEGER: row[col_name] = sqlite3_column_int64(stmt, i); break;
        case SQLITE_FLOAT:   row[col_name] = sqlite3_column_double(stmt, i); break;
        case SQLITE_TEXT:    row[col_name] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)); break;
        case SQLITE_NULL:    row[col_name] = nullptr; break;
        default:             row[col_name] = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, i)); break;
      }
    }
    rows.push_back(std::move(row));
  }
  sqlite3_finalize(stmt);
  return rows;
}

Result<int> Database::execute(const std::string& sql) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string msg = err_msg;
    sqlite3_free(err_msg);
    return std::unexpected(Error{500, std::move(msg)});
  }
  return sqlite3_changes(impl_->db);
}

Result<long long> Database::insert(const std::string& sql) {
  auto result = execute(sql);
  if (!result) return std::unexpected(result.error());
  return sqlite3_last_insert_rowid(impl_->db);
}

// ═══════════════════════════════════════════════════════════
// Server
// ═══════════════════════════════════════════════════════════
Server::Server(std::string_view db_path, int port)
  : db_(db_path), port_(port) {}

json Server::error_response(int code, std::string_view message) const {
  return {{"error", true}, {"code", code}, {"message", message.data()}};
}

void Server::setup_routes() {
  // CORS preflight
  svr_.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.status = 204;
  });

  // Health
  svr_.Get("/api/health", [this](const httplib::Request&, httplib::Response& res) {
    handle_health(httplib::Request{}, res);
  });

  // Items CRUD
  svr_.Get("/api/items",  [this](const httplib::Request& req, httplib::Response& res) {
    handle_get_items(req, res);
  });
  svr_.Get(R"(/api/items/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
    handle_get_item(req, res);
  });
  svr_.Post("/api/items", [this](const httplib::Request& req, httplib::Response& res) {
    handle_post_item(req, res);
  });
  svr_.Put(R"(/api/items/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
    handle_put_item(req, res);
  });
  svr_.Delete(R"(/api/items/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
    handle_delete_item(req, res);
  });

  // 404 catch-all
  svr_.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
    try { std::rethrow_exception(ep); }
    catch (std::exception& e) {
      res.status = 500;
      res.set_content(json{{"error", true}, {"message", e.what()}}.dump(), "application/json");
    }
  });

  spdlog::info("Routes configured successfully");
}

int Server::run() {
  spdlog::info("Starting C++ REST API server on port {}", port_);
  svr_.set_read_timeout(30);
  svr_.set_write_timeout(10);
  svr_.set_idle_interval(0);  // no keep-alive idle timeout
  svr_.listen("0.0.0.0", port_);
  return 0;
}

// ── Health ─────────────────────────────────────────────────
void Server::handle_health(const httplib::Request&, httplib::Response& res) {
  json response = {
    {"status", "ok"},
    {"version", "1.0.0"},
    {"timestamp", std::time(nullptr)}
  };
  res.set_content(response.dump(2), "application/json");
}

// ── GET /api/items ─────────────────────────────────────────
void Server::handle_get_items(const httplib::Request& req, httplib::Response& res) {
  auto params = req.params;

  // Pagination
  int page = 1, per_page = 20;
  if (params.contains("page")) page = std::stoi(params.at("page"));
  if (params.contains("per_page")) per_page = std::stoi(params.at("per_page"));
  int offset = (page - 1) * per_page;

  // Search
  std::string where;
  if (params.contains("search")) {
    auto escaped = params.at("search");
    // Simple escape for SQLite
    size_t pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
      escaped.replace(pos, 1, "''");
      pos += 2;
    }
    where = std::format("WHERE name LIKE '%{}%' OR description LIKE '%{}%'", escaped, escaped);
  }

  // Count
  auto count_result = db_.query(std::format("SELECT COUNT(*) as total FROM items {}", where));
  int total = 0;
  if (count_result && !count_result->empty()) {
    total = (*count_result)[0]["total"].get<int>();
  }

  // Fetch
  auto sql = std::format(
    "SELECT * FROM items {} ORDER BY created_at DESC LIMIT {} OFFSET {}",
    where, per_page, offset
  );
  auto result = db_.query(sql);

  if (!result) {
    res.status = 500;
    res.set_content(error_response(500, result.error().message).dump(), "application/json");
    return;
  }

  json body = {
    {"data",        *result},
    {"page",        page},
    {"per_page",    per_page},
    {"total",       total},
    {"total_pages", (total + per_page - 1) / per_page}
  };
  res.set_content(body.dump(2), "application/json");
}

// ── GET /api/items/:id ─────────────────────────────────────
void Server::handle_get_item(const httplib::Request& req, httplib::Response& res) {
  int id = std::stoi(req.matches[1]);
  auto result = db_.query(std::format("SELECT * FROM items WHERE id = {}", id));

  if (!result) {
    res.status = 500;
    res.set_content(error_response(500, result.error().message).dump(), "application/json");
    return;
  }

  if (result->empty()) {
    res.status = 404;
    res.set_content(error_response(404, "Item not found").dump(), "application/json");
    return;
  }

  res.set_content((*result)[0].dump(2), "application/json");
}

// ── POST /api/items ────────────────────────────────────────
void Server::handle_post_item(const httplib::Request& req, httplib::Response& res) {
  json body;
  try {
    body = json::parse(req.body);
  } catch (json::parse_error&) {
    res.status = 400;
    res.set_content(error_response(400, "Invalid JSON body").dump(), "application/json");
    return;
  }

  if (!body.contains("name") || !body["name"].is_string()) {
    res.status = 400;
    res.set_content(error_response(400, "Field 'name' (string) is required").dump(), "application/json");
    return;
  }

  std::string name = body["name"];
  std::string description = body.value("description", "");

  // Escape single quotes
  auto esc = [](std::string& s) {
    size_t p = 0;
    while ((p = s.find('\'', p)) != std::string::npos) { s.replace(p, 1, "''"); p += 2; }
  };
  esc(name);
  esc(description);

  auto id_result = db_.insert(std::format(
    "INSERT INTO items (name, description) VALUES ('{}', '{}')",
    name, description
  ));

  if (!id_result) {
    res.status = 500;
    res.set_content(error_response(500, id_result.error().message).dump(), "application/json");
    return;
  }

  auto fetch = db_.query(std::format("SELECT * FROM items WHERE id = {}", *id_result));
  res.status = 201;
  res.set_content(fetch.has_value() && !fetch->empty() ? (*fetch)[0].dump(2) : json{{"id", *id_result}}.dump(2), "application/json");
}

// ── PUT /api/items/:id ─────────────────────────────────────
void Server::handle_put_item(const httplib::Request& req, httplib::Response& res) {
  int id = std::stoi(req.matches[1]);
  json body;
  try {
    body = json::parse(req.body);
  } catch (json::parse_error&) {
    res.status = 400;
    res.set_content(error_response(400, "Invalid JSON body").dump(), "application/json");
    return;
  }

  if (!body.contains("name") || !body["name"].is_string()) {
    res.status = 400;
    res.set_content(error_response(400, "Field 'name' (string) is required").dump(), "application/json");
    return;
  }

  std::string name = body["name"];
  std::string description = body.value("description", "");

  auto esc = [](std::string& s) {
    size_t p = 0;
    while ((p = s.find('\'', p)) != std::string::npos) { s.replace(p, 1, "''"); p += 2; }
  };
  esc(name);
  esc(description);

  auto result = db_.execute(std::format(
    "UPDATE items SET name = '{}', description = '{}', updated_at = datetime('now') WHERE id = {}",
    name, description, id
  ));

  if (!result) {
    res.status = 500;
    res.set_content(error_response(500, result.error().message).dump(), "application/json");
    return;
  }

  if (*result == 0) {
    res.status = 404;
    res.set_content(error_response(404, "Item not found").dump(), "application/json");
    return;
  }

  auto fetch = db_.query(std::format("SELECT * FROM items WHERE id = {}", id));
  res.set_content(fetch.has_value() && !fetch->empty() ? (*fetch)[0].dump(2) : json{{"id", id}}.dump(2), "application/json");
}

// ── DELETE /api/items/:id ──────────────────────────────────
void Server::handle_delete_item(const httplib::Request& req, httplib::Response& res) {
  int id = std::stoi(req.matches[1]);
  auto result = db_.execute(std::format("DELETE FROM items WHERE id = {}", id));

  if (!result) {
    res.status = 500;
    res.set_content(error_response(500, result.error().message).dump(), "application/json");
    return;
  }

  if (*result == 0) {
    res.status = 404;
    res.set_content(error_response(404, "Item not found").dump(), "application/json");
    return;
  }

  json body = {{"message", "Item deleted successfully"}, {"id", id}};
  res.set_content(body.dump(2), "application/json");
}

// ═══════════════════════════════════════════════════════════
// Entry point
// ═══════════════════════════════════════════════════════════
}  // namespace api

int main(int argc, char** argv) {
  // Initialize logger
  auto console = spdlog::stdout_color_mt("api");
  spdlog::set_default_logger(console);
  spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

  int port = 8080;
  std::string db_path = "data/items.db";

  // Parse CLI args
  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    else if (arg == "--db" && i + 1 < argc) db_path = argv[++i];
    else if (arg == "--help" || arg == "-h") {
      std::println("Usage: cpp-rest-api [--port PORT] [--db PATH]");
      return 0;
    }
  }

  try {
    // Ensure data directory exists
    std::filesystem::create_directories(
      std::filesystem::path(db_path).parent_path()
    );

    api::Server server(db_path, port);
    server.setup_routes();
    spdlog::info("Server ready at http://0.0.0.0:{}/api", port);
    spdlog::info("Endpoints:");
    spdlog::info("  GET    /api/health");
    spdlog::info("  GET    /api/items");
    spdlog::info("  GET    /api/items/:id");
    spdlog::info("  POST   /api/items");
    spdlog::info("  PUT    /api/items/:id");
    spdlog::info("  DELETE /api/items/:id");
    return server.run();
  } catch (const std::exception& e) {
    spdlog::critical("Fatal error: {}", e.what());
    return 1;
  }
}
