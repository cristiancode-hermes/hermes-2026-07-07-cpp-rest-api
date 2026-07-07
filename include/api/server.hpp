#ifndef CPP_REST_API_SERVER_HPP
#define CPP_REST_API_SERVER_HPP

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <expected>
#include <sqlite3.h>

namespace api {

using json = nlohmann::json;

// ── Error handling ────────────────────────────────────────
struct Error {
  int         code;
  std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

// ── Database wrapper ──────────────────────────────────────
class Database {
public:
  explicit Database(std::string_view path);
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) noexcept;
  Database& operator=(Database&&) noexcept;

  Result<std::vector<json>> query(const std::string& sql);
  Result<int>               execute(const std::string& sql);
  Result<long long>         insert(const std::string& sql);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// ── REST Server ───────────────────────────────────────────
class Server {
public:
  Server(std::string_view db_path, int port = 8080);

  void setup_routes();
  int  run();

private:
  // Items (generic CRUD example)
  void handle_get_items   (const httplib::Request&, httplib::Response&);
  void handle_get_item    (const httplib::Request&, httplib::Response&);
  void handle_post_item   (const httplib::Request&, httplib::Response&);
  void handle_put_item    (const httplib::Request&, httplib::Response&);
  void handle_delete_item (const httplib::Request&, httplib::Response&);

  // Health
  void handle_health      (const httplib::Request&, httplib::Response&);

  json error_response(int code, std::string_view message) const;

  httplib::Server svr_;
  Database        db_;
  int             port_;
};

}  // namespace api

#endif  // CPP_REST_API_SERVER_HPP
