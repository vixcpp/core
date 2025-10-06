# vixcpp/core

![C++](https://img.shields.io/badge/C++20-Standard-blue)
![License](https://img.shields.io/badge/License-MIT-green)

**vix.cpp/core** – The core module of the **vix.cpp** framework.
Provides the HTTP server, router, request handlers, JSON utilities, and middleware system. All other modules in **vix.cpp** build on top of this.

---

## Features

- **HTTP Server** – High-performance, async server using Boost.Asio & Beast.
- **Router** – Supports path parameters (`/users/{id}`) and HTTP method matching.
- **Request Handlers** – Flexible signatures for JSON & text responses.
- **JSON Utilities** – Built-in [nlohmann/json](https://github.com/nlohmann/json) support.
- **Middleware System** – Easily extendable for logging, authentication, etc.
- **Graceful Shutdown** – Signal handling and thread-safe shutdown.
- **Optimized for Performance** – Minimal allocations, `std::string_view`, fast path extraction.

---

## Architecture Diagram

```text
+--------------------+
|      App           |
| - Manages routes   |
| - Runs HTTP server |
+---------+----------+
          |
          v
+--------------------+
|     Router         |
| - Stores routes    |
| - Matches requests |
+---------+----------+
          |
          v
+--------------------+
| RequestHandler<T>  |
| - Executes handler |
| - Extracts params  |
+---------+----------+
          |
          v
+--------------------+
| ResponseWrapper    |
| - JSON / Text      |
| - Status management|
+--------------------+
```

---

## Installation

```bash
git clone https://github.com/vixcpp/core.git
cd core
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Prerequisites: C++20, CMake ≥3.20, Boost libraries, nlohmann/json.

---

## Quick Usage

```cpp
#include <vix/core.h>

int main() {
    Vix::App app;

    app.get("/hello", [](auto &req, auto &res) {
        res.json({{"message", "Hello, World!"}});
    });

    app.get("/users/{id}", [](auto &req, auto &res, auto &params) {
        res.json({{"user_id", params["id"]}});
    });

    app.run(8080);
}
```

- Supports `GET`, `POST`, `PUT`, `DELETE`.
- Path parameters automatically extracted.
- `res.json(...)`, `res.text(...)`, `res.status(...)` helpers.

---

## Performance

```text
wrk -t8 -c100 -d30s http://localhost:8080/hello
Requests/sec: ~48k, Latency: ~2ms

wrk -t8 -c100 -d30s http://localhost:8080/users/1
Requests/sec: ~44k, Latency: ~2.2ms
```

Optimizations: `std::string_view`, fast path extraction, efficient JSON serialization.

---

## Contributing

1. Fork → create a feature branch → commit → push → PR.
2. Follow the [core coding guidelines](https://github.com/vixcpp/core).

---

## License

[MIT](../../LICENSE)
