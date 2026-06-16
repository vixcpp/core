# Vix.cpp Core Module

Native application foundation for Vix.cpp.

The Core module provides the main runtime layer used to build HTTP applications in C++ with routes, handlers, middleware, requests, responses, configuration, TLS, static files, templates, runtime execution, async I/O integration, and server lifecycle management.

## Documentation

Full documentation is available here:

https://docs.vixcpp.com/modules/core/

API reference:

https://docs.vixcpp.com/modules/core/api-reference

Engineering article about the official Core benchmark baseline:

https://blog.vixcpp.com/posts/vix-core/vix-core-benchmark-baseline-v263

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
- Server lifecycle management
- Performance benchmarks for core runtime paths

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

## Route groups

```cpp
#include <vix.hpp>

int main()
{
  vix::App app;

  app.group("/api", [](auto api)
  {
    api.get("/status", [](vix::Request &req, vix::Response &res)
    {
      (void)req;

      res.json({
        {"status", "ok"},
        {"scope", "api"}
      });
    });

    api.get("/users/{id}", [](vix::Request &req, vix::Response &res)
    {
      res.json({
        {"id", req.param("id")},
        {"name", "Vix user"}
      });
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
  -> Runtime
  -> Async I/O
```

Core connects the public application API with the runtime, async I/O, HTTP server, routing layer, request parser, response system, middleware pipeline, and server lifecycle.

## Performance benchmarks

Core includes an official benchmark suite used to track performance across releases and detect regressions before changes are merged or released.

The benchmark suite covers:

```text
runtime.task
runtime.queue
runtime.scheduler
runtime.worker

executor.submit
executor.post
executor.metrics

router.match
router.registration

http.request
http.response

session.fake_transport

app.route_registration
app.group_registration
```

The official v2.6.3 benchmark baseline was generated in Release mode on Linux x86_64 with GCC 13.3.0.

Read the full engineering note here:

https://blog.vixcpp.com/posts/vix-core/vix-core-benchmark-baseline-v263

### Build benchmarks

Official benchmark numbers must be generated from a Release build.

From `modules/core`:

```bash
vix build --clean --preset release --build-target all -v -- \
  -DVIX_CORE_BUILD_BENCHMARKS=ON \
  -DVIX_CORE_BUILD_TESTS=ON
```

Benchmark binaries are generated under:

```text
build-release/benchmarks/core/
```

### Run one benchmark

```bash
./build-release/benchmarks/core/core_router_match_bench
```

Write JSON output:

```bash
./build-release/benchmarks/core/core_router_match_bench \
  benchmarks/results/dev/core_router_match_bench.json
```

### Run the full benchmark suite

```bash
./scripts/run_core_benchmarks.sh \
  --bin-dir build-release/benchmarks/core \
  --out-dir benchmarks/results/dev \
  --version dev \
  --runner local \
  --machine local
```

### Create an official baseline

```bash
./scripts/run_core_benchmarks.sh \
  --bin-dir build-release/benchmarks/core \
  --out-dir benchmarks/baselines/v2.6.3 \
  --version v2.6.3 \
  --runner official \
  --machine vix-bench-runner-1
```

### Compare against a baseline

```bash
./scripts/compare_core_benchmarks.py \
  benchmarks/baselines/v2.6.3 \
  benchmarks/results/current
```

By default, the comparison uses:

```text
median_ops_per_sec
```

Higher is better.

The default thresholds are:

```text
WARN = -5%
FAIL = -10%
```

Use custom thresholds:

```bash
./scripts/compare_core_benchmarks.py \
  benchmarks/baselines/v2.6.3 \
  benchmarks/results/current \
  --warn 5 \
  --fail 10
```

## Benchmark rules

Use `dev` or `dev-ninja` builds for development, debugging, and compile validation.

Use `release` builds for performance measurements.

```text
dev / debug = compile, test, debug
release     = measure performance
```

Official baselines should not be generated from debug builds.

For stable results:

- use the same machine when possible,
- avoid heavy background tasks,
- keep the runner name stable,
- keep the machine name stable,
- store baselines under versioned directories,
- compare future changes against the official baseline.

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

To build Core with tests explicitly enabled:

```bash
vix build --build-target all -v -- \
  -DVIX_CORE_BUILD_TESTS=ON
```

## Useful links

- Core documentation: https://docs.vixcpp.com/modules/core/
- Core API reference: https://docs.vixcpp.com/modules/core/api-reference
- Core benchmark baseline article: https://blog.vixcpp.com/posts/vix-core/vix-core-benchmark-baseline-v263
- Build command: https://docs.vixcpp.com/cli/build
- Tests command: https://docs.vixcpp.com/cli/tests
- Documentation: https://docs.vixcpp.com/
- Engineering notes: https://blog.vixcpp.com/
- Registry: https://registry.vixcpp.com/
- GitHub: https://github.com/vixcpp/vix

## License

MIT License.

See [`LICENSE`](../../LICENSE) for details.
