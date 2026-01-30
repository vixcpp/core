# Vix Core

Fast HTTP runtime • Router • Request/Response • JSON helpers • Production-ready primitives

`core` is the foundation of **Vix.cpp**.
It provides the HTTP runtime primitives used across the ecosystem:

* `vix::App` — the main HTTP application
* routing (`get/post/...`) with path params
* `Request` / `Response` wrappers
* JSON helpers (`vix::json`) for fast structured responses
* query params, headers, body access
* predictable status codes + early return patterns

If Vix.cpp is a runtime, **core is its execution engine for HTTP**.

---

## Quick start

Run the showcase example:

```bash
vix run examples/vix_routes_showcase.cpp
```

Then hit a few endpoints:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/users/42
curl -i "http://127.0.0.1:8080/search?q=vix&page=2&limit=5"
curl -i http://127.0.0.1:8080/headers
curl -i http://127.0.0.1:8080/status/404
```

---

## Minimal HTTP server

```cpp
#include <vix.hpp>

using namespace vix;

int main()
{
  App app;

  app.get("/", [](Request&, Response& res) {
    res.json({"message", "Hello, Vix!"});
  });

  app.run(8080);
}
```

---

## Routing model

Vix routes are designed to be:

* **simple** to read
* **copy/paste friendly** for real projects
* explicit about status codes and early returns

### Basic routes

```cpp
app.get("/hello", [](const Request&, Response&) {
  return vix::json::o("message", "Hello", "id", 20);
});

app.get("/txt", [](const Request&, Response&) {
  return "Hello world";
});
```

### Status codes

```cpp
app.get("/status/{code}", [](Request& req, Response& res) {
  const int code = /* parse */ 200;
  res.status(code).json({
    "status", code,
    "ok", (code >= 200 && code < 300)
  });
});
```

### Path params

```cpp
app.get("/users/{id}", [](Request& req, Response& res) {
  const std::string id = req.param("id", "0");

  if (id == "0") {
    res.status(404).json({"error", "User not found", "id", id});
    return;
  }

  res.json({"id", id, "vip", (id == "42")});
});
```

### Query params

```cpp
app.get("/search", [](Request& req, Response& res) {
  const std::string q = req.query_value("q", "");
  const std::string page = req.query_value("page", "1");
  const std::string limit = req.query_value("limit", "10");

  res.json({
    "q", q,
    "page", page,
    "limit", limit
  });
});
```

### Headers

```cpp
app.get("/headers", [](Request& req, Response& res) {
  res.json({
    "host", req.header("Host"),
    "user_agent", req.header("User-Agent"),
    "accept", req.header("Accept")
  });
});
```

---

## Request body + JSON parsing

Core exposes the raw body and JSON parsing helpers.

```cpp
app.get("/echo/body", [](Request& req, Response& res) {
  const std::string body = req.body();
  res.json({"bytes", (long long)body.size(), "body", body});
});

app.get("/echo/json", [](Request& req, Response& res) {
  const auto& j = req.json();
  res.json(j);
});
```

For defensive field access, the showcase demonstrates patterns like:

* check `is_object()`
* check `contains()` + type
* return defaults on missing fields

---

## Mixed behaviors (send + return)

Vix supports both:

* explicit response (`res.send`, `res.json`)
* return-value auto-send (string or JSON object)

If you **already sent** a response explicitly, the returned value is ignored.

```cpp
app.get("/mix", [](Request&, Response& res) {
  res.status(201).send("Created");
  return vix::json::o("ignored", true);
});
```

This makes it easy to write handlers in a style similar to Node/FastAPI:

* short happy path
* early return on errors

---

## Showcase example

The recommended living reference for the public API style is:

* `examples/vix_routes_showcase.cpp`

It includes:

* simple routes
* JSON responses (flat + nested)
* status codes
* path params (`/users/{id}`)
* query params (`?page=1&limit=20`)
* headers
* request body + JSON parsing
* mixed behaviors (send + return)
* many copy/paste route patterns

---

## How core fits in the umbrella

* `core` powers the HTTP runtime (`vix::App`, routing, request/response)
* `middleware` attaches on top of core (security, parsers, auth, cache)
* `websocket` can run standalone or alongside HTTP
* `p2p_http` uses core to expose P2P control endpoints

---

## Directory layout

Typical layout:

```
modules/core/
│
├─ include/vix/
│  ├─ http/...
│  ├─ router/...
│  ├─ server/...
│  ├─ session/...
│  ├─ config/...
│  ├─ utils/...
│  └─ vix.hpp            # umbrella include for HTTP runtime
│
└─ examples/
   └─ vix_routes_showcase.cpp
```

---

## License

MIT — same as Vix.cpp

Repository: [https://github.com/vixcpp/vix](https://github.com/vixcpp/vix)
