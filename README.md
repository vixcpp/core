# Vix.cpp Core Module

Native application foundation for Vix.cpp.

The Core module provides the main runtime layer used to build HTTP applications in C++ with routes, handlers, middleware, requests, responses, configuration, TLS, static files, templates, and server lifecycle management.

## Documentation

Full documentation is available here:

https://docs.vixcpp.com/modules/core/

API reference:

https://docs.vixcpp.com/modules/core/api-reference

## What Core provides

- `vix::App`
- HTTP server
- Routing system
- Request and response objects
- Middleware
- Route groups
- Static files
- Templates
- Configuration
- TLS support
- Runtime executor integration
- Async I/O integration

## Public header

```cpp
#include <vix.hpp>
```

You can also include Core directly:

```cpp
#include <vix/core.hpp>
```

## Minimal HTTP server

```cpp
#include <vix.hpp>

int main()
{
  vix::App app;

  app.get("/", [](vix::Request &req, vix::Response &res)
  {
    (void)req;

    res.text("Hello from Vix");
  });

  app.run(8080);

  return 0;
}
```

Run:

```bash
vix run main.cpp
```

Open:

```text
http://localhost:8080
```

## JSON API example

```cpp
#include <vix.hpp>

int main()
{
  vix::App app;

  app.get("/api/status", [](vix::Request &req, vix::Response &res)
  {
    (void)req;

    res.json({
      {"status", "ok"},
      {"server", "Vix.cpp"}
    });
  });

  app.run(8080);

  return 0;
}
```

## Core architecture

```text
App
  -> Router
  -> HTTPServer
  -> Session
  -> Transport
  -> RuntimeExecutor
```

Core connects the application API with the runtime, async I/O, HTTP server, routing layer, and response system.

## Build

Contributors should use the Vix CLI to build this module.

Vix wraps the C++ build workflow with project detection, presets, Ninja builds, clean logs, caching, and focused diagnostics. This helps avoid hidden C++ build issues and keeps the contributor workflow consistent.

### Build the project

```bash
git clone https://github.com/vixcpp/vix.git
cd vix
vix build
```

### Build all targets

Use this before running the full test suite, install workflows, or release checks:

```bash
vix build --build-target all
```

### Clean rebuild

Use this when the local CMake cache or build directory may be stale:

```bash
vix build --clean
```

### Release build

```bash
vix build --preset release
```

## Tests

Build all targets first, then run tests:

```bash
vix build --build-target all
vix tests
```

Before opening a pull request, use:

```bash
vix fmt --check
vix build --build-target all
vix tests
```

## Useful links

- Core documentation: https://docs.vixcpp.com/modules/core/
- Core API reference: https://docs.vixcpp.com/modules/core/api-reference
- Build command: https://docs.vixcpp.com/cli/build
- Tests command: https://docs.vixcpp.com/cli/tests
- Documentation: https://docs.vixcpp.com/
- Engineering notes: https://blog.vixcpp.com/
- Registry: https://registry.vixcpp.com/
- GitHub: https://github.com/vixcpp/vix

## License

MIT License.

See [`LICENSE`](../../LICENSE) for details.
