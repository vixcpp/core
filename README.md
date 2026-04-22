# Core — Vix.cpp

Build real backends in C++.
One runtime. One system. No glue.

## What is this?

The Vix core module is the foundation of the runtime.

It provides:

- HTTP server
- routing system
- request / response model
- async execution
- configuration system (.env)
- OpenAPI generation
- offline API documentation (Swagger UI)

Everything you need to build APIs. Nothing extra.

## Why this exists

Most C++ backends are built with:

- multiple libraries
- inconsistent abstractions
- manual wiring
- fragile integrations

Vix core gives you a coherent backend system:

- one router
- one request model
- one runtime
- one config system

No fragmentation.

## Install

```bash
curl -fsSL https://vixcpp.com/install.sh | bash
```

## Minimal HTTP server

```cpp
#include <vix.hpp>

int main()
{
  auto exec = std::make_shared<vix::executor::RuntimeExecutor>();

  vix::App app{exec};

  app.get("/", [](auto&, auto& res) {
    res.json({
      "message", "Hello from Vix",
      "framework", "Vix.cpp"
    });
  });

  app.run(8080);
}
```

## Request / Response model

```cpp
app.get("/hello/{name}", [](vix::Request& req, vix::Response& res) {
  res.json({
    "message", "Hello " + req.param("name")
  });
});
```

- `req.param()` → path params
- `req.query()` → query string
- `res.json()` → JSON response
- `res.text()` → text response
- `res.file()` → static files

## Async model

Handlers are async-ready:

```cpp
vix::async::core::task<void> handle_request(
    const Request& req,
    Response& res);
```

No blocking required.
The runtime handles scheduling.

## Configuration (.env)

```cpp
vix::config::Config cfg{".env"};

int port = cfg.getServerPort();
bool async = cfg.getLogAsync();
```

No JSON. No parsing boilerplate.

## OpenAPI (built-in)

```cpp
auto spec = vix::openapi::build_from_router(router);
```

- no annotations required
- stable operationId
- full JSON output

## Offline API documentation

```cpp
vix::openapi::register_openapi_and_docs(router);
```

This adds:

- GET /openapi.json
- GET /docs
- GET /docs/index.html
- GET /docs/swagger-ui.css
- GET /docs/swagger-ui-bundle.js

All assets are embedded and served locally.

## Example

Open:

```
http://localhost:8080/docs
```

You get:

- interactive API UI
- live requests
- no internet required

## Design philosophy

Core is designed for:

- clarity over abstraction
- explicit over magic
- runtime consistency
- production readiness

No hidden layers.
No implicit behavior.

## What you can build

- REST APIs
- microservices
- backend platforms
- internal tools
- realtime systems (with websocket module)

## Examples

```
examples/http/
examples/websocket/
```

## Key idea

Vix core is not a framework layer.
It is the runtime foundation.

Everything else builds on top of it.

## Learn more

Learn more about the Vix runtime in the documentation.

## License

MIT License.

