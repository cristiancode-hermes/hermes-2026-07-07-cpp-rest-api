#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <thread>
#include <chrono>

using json = nlohmann::json;

// ── Unit Tests ────────────────────────────────────────────

TEST_CASE("JSON serialization roundtrip", "[json]") {
  json obj = {
    {"name", "test-item"},
    {"description", "A test item"},
    {"count", 42}
  };

  std::string serialized = obj.dump();
  json parsed = json::parse(serialized);

  REQUIRE(parsed["name"] == "test-item");
  REQUIRE(parsed["description"] == "A test item");
  REQUIRE(parsed["count"]  == 42);
}

TEST_CASE("JSON error response format", "[json]") {
  json error = {
    {"error", true},
    {"code", 404},
    {"message", "Not found"}
  };

  REQUIRE(error["error"].get<bool>() == true);
  REQUIRE(error["code"].get<int>()   == 404);
  REQUIRE(error["message"]           == "Not found");
}

// ── Integration Tests (requires running server) ───────────

struct ServerFixture {
  httplib::Client cli{"http://localhost:8080"};

  ServerFixture() {
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
};

TEST_CASE_METHOD(ServerFixture, "Health endpoint returns 200", "[integration]") {
  auto res = cli.Get("/api/health");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = json::parse(res->body);
  REQUIRE(body["status"] == "ok");
}

TEST_CASE_METHOD(ServerFixture, "Create and retrieve item", "[integration]") {
  json new_item = {{"name", "integration-test"}, {"description", "Created by Catch2"}};

  // POST
  auto post_res = cli.Post("/api/items", new_item.dump(), "application/json");
  REQUIRE(post_res);
  REQUIRE(post_res->status == 201);

  auto created = json::parse(post_res->body);
  REQUIRE(created["name"] == "integration-test");

  int id = created["id"].get<int>();
  REQUIRE(id > 0);

  // GET by id
  auto get_res = cli.Get(("/api/items/" + std::to_string(id)).c_str());
  REQUIRE(get_res);
  REQUIRE(get_res->status == 200);

  auto fetched = json::parse(get_res->body);
  REQUIRE(fetched["name"] == "integration-test");
}

TEST_CASE_METHOD(ServerFixture, "List items returns paginated response", "[integration]") {
  auto res = cli.Get("/api/items?page=1&per_page=10");
  REQUIRE(res);
  REQUIRE(res->status == 200);

  auto body = json::parse(res->body);
  REQUIRE(body.contains("data"));
  REQUIRE(body.contains("page"));
  REQUIRE(body.contains("total_pages"));
  REQUIRE(body["page"].get<int>() == 1);
}

TEST_CASE_METHOD(ServerFixture, "Update item", "[integration]") {
  // Create first
  auto post_res = cli.Post("/api/items",
    json{{"name", "original"}, {"description", "Before update"}}.dump(),
    "application/json");
  REQUIRE(post_res);
  int id = json::parse(post_res->body)["id"].get<int>();

  // Update
  auto put_res = cli.Put(("/api/items/" + std::to_string(id)).c_str(),
    json{{"name", "updated"}, {"description", "After update"}}.dump(),
    "application/json");
  REQUIRE(put_res);
  REQUIRE(put_res->status == 200);

  auto updated = json::parse(put_res->body);
  REQUIRE(updated["name"] == "updated");
}

TEST_CASE_METHOD(ServerFixture, "Delete item", "[integration]") {
  // Create
  auto post_res = cli.Post("/api/items",
    json{{"name", "delete-me"}}.dump(), "application/json");
  REQUIRE(post_res);
  int id = json::parse(post_res->body)["id"].get<int>();

  // Delete
  auto del_res = cli.Delete(("/api/items/" + std::to_string(id)).c_str());
  REQUIRE(del_res);
  REQUIRE(del_res->status == 200);

  // Verify gone
  auto get_res = cli.Get(("/api/items/" + std::to_string(id)).c_str());
  REQUIRE(get_res);
  REQUIRE(get_res->status == 404);
}

TEST_CASE_METHOD(ServerFixture, "Invalid JSON returns 400", "[integration]") {
  auto res = cli.Post("/api/items", "not-json", "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 400);
}

TEST_CASE_METHOD(ServerFixture, "Missing name field returns 400", "[integration]") {
  auto res = cli.Post("/api/items",
    json{{"description", "no name"}}.dump(), "application/json");
  REQUIRE(res);
  REQUIRE(res->status == 400);
}

// ── Benchmarks ────────────────────────────────────────────

TEST_CASE("JSON parse benchmark", "[benchmark]") {
  std::string large_json = R"({
    "data": [
      {"id": 1, "name": "item1", "description": "A very long description that goes on and on..."},
      {"id": 2, "name": "item2", "description": "Another long description for benchmarking purposes"},
      {"id": 3, "name": "item3", "description": "Yet another item with a verbose description field"}
    ],
    "page": 1,
    "per_page": 20,
    "total": 100,
    "total_pages": 5
  })";

  BENCHMARK("Parse 5KB JSON") {
    return json::parse(large_json);
  };
}
