# Changelog

All notable changes to `@vix/core` are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added

- Added a compile-time `VIX_BENCH_MODE` fast path for `GET /bench` that serves the benchmark response directly from the session layer.

### Changed

- Reduced HTTP benchmark hot-path allocations by avoiding request header lookup temporaries and moving parsed headers into `Request` directly.
- Cached HTTP `Date` header generation to avoid per-response formatting overhead.
- Reduced `/bench` response bytes and reserved the session read buffer for common request sizes.
- Switched async TCP writes to `async_write_some`; the session `write_all()` loop continues to guarantee complete writes.
- Enabled `TCP_NODELAY` on accepted TCP sockets.

### Fixed

- Suppressed normal client disconnect noise (`EOF`, connection reset) from session fatal-error logs.

## [2.11.0] - 2026-05-24

### Changed

- Removed legacy threadpool implementation and direct threadpool includes from core.
- Moved developer helper dependencies onto the `io` module.
- Removed old meta helpers from core.
- Updated OpenAPI documentation UI design.

### Fixed

- Classified response-write disconnects as normal network disconnects.
- Fixed standalone dependency layout and runtime benchmark linkage.

### Tests

- Added coverage for response write disconnect handling.

## [2.10.0] - 2026-05-06

### Changed

- Aligned core HTTP JSON handling with the `vix::json` API.

## [2.9.0] - 2026-04-23

### Added

- Added a simple interactive input API.

## [2.8.0] - 2026-04-22

### Added

- Added the stable options-based print API.
- Added an OpenAPI `/docs` documentation section with offline API documentation highlights.

## [2.7.4] - 2026-04-21

### Fixed

- Closed HTTP sessions on EOF instead of looping.

## [2.7.3] - 2026-04-21

### Fixed

- Stabilized the HTTP request path and benchmark helpers.

## [2.7.2] - 2026-04-21

### Changed

- Optimized the benchmark-mode HTTP request path.

## [2.7.1] - 2026-04-21

### Changed

- Restored multi-threaded HTTP I/O.
- Removed debug traces from the HTTP hot path.

## [2.7.0] - 2026-04-21

### Changed

- Simplified `vix.hpp` into a minimal umbrella aggregator.
- Improved HTTP/WebSocket runtime lifecycle handling.

### Fixed

- Stabilized app and HTTP server shutdown.
- Removed `vix_warnings` and sanitizer leakage from exported targets.
- Switched umbrella includes to modular JSON and utils headers.

## [2.6.0] - 2026-04-18

### Fixed

- Corrected `static_dir` routing and root mount matching.

## [2.5.0] - 2026-04-18

### Removed

- Removed `Config::getInstance()`.

### Changed

- Enforced explicit configuration usage throughout core.

## [2.4.0] - 2026-04-17

### Changed

- Switched core configuration to `.env`-based loading through `vix::env`.

## [2.3.0] - 2026-04-17

### Changed

- Renamed the native HTTP namespace from `vhttp` to `http` across core.
- Added package configuration and unified export logic.

### Fixed

- Updated the bench route to use the `ResponseWrapper` API.

## [2.2.0] - 2026-04-10

### Added

- Improved configuration handling and structure.

## [2.1.1] - 2026-04-05

### Fixed

- Eliminated warnings and improved safety in console and session code.

## [2.1.0] - 2026-03-31

### Added

- Added template rendering with `res.render` and view integration.
- Added the format API with a pure C++ test suite.

### Changed

- Optimized the HTTP hot path and runtime scheduler.
- Improved scheduler, run queue, and task execution behavior.
- Switched core execution integration from the older threadpool model toward the runtime executor.
- Centralized traits and removed duplicate helpers.

### Fixed

- Improved worker shutdown and task rescheduling safety.
- Improved Windows compatibility and formatting behavior.
- Removed noisy internal debug logs from the shutdown path.
- Replaced Unicode box drawing with cross-platform fallback output.

## [2.0.2] - 2026-03-25

### Fixed

- Maintenance release following the native HTTP migration.

## [2.0.1] - 2026-03-25

### Fixed

- Maintenance release following the native HTTP migration.

## [2.0.0] - 2026-03-24

### Added

- Introduced the native `vix::http` request/response layer.
- Added native `Request`, `Response`, and `ResponseWrapper` APIs.
- Added async-first HTTP server architecture powered by `vix::async`.
- Added runtime-backed HTTP server execution.
- Added standalone runtime tests and benchmark support.

### Changed

- Reworked middleware integration for the native HTTP model.
- Slimmed the session hot path and added runtime benchmark mode.
- Routed request handling through `RuntimeExecutor`.
- Improved HTTP server shutdown idempotency and listener startup determinism.

### Removed

- Removed the Boost.Beast HTTP stack from core.
- Removed old Beast-based public APIs.

### Fixed

- Silenced normal client disconnect EOF logs.
- Made runtime executor, HTTP server, and threadpool shutdown safer.
- Made app shutdown idempotent.

## [1.13.3] - 2026-01-12

### Changed

- Extracted cache and sync functionality into dedicated modules.
- Removed unused code left in core after modular cleanup.

## [1.13.2] - 2026-01-03

### Changed

- Removed logger context and console synchronization from HTTP access logs.

### Fixed

- Bound the HTTP acceptor during `run()` so `App::listen(port)` honors the requested port.

## [1.13.1] - 2026-01-02

### Fixed

- Prevented Boost targets from leaking into exported CMake packages.

## [1.13.0] - 2026-01-02

### Fixed

- Printed startup logs after the banner.
- Decoupled startup logging from access logs.

## [1.12.0] - 2025-12-25

### Changed

- Improved app middleware mounting and routing flow.

## [1.11.0] - 2025-12-24

### Added

- Added foundational middleware primitives in core: `Context`, `Next`, `NextOnce`, `Hooks`, `Result`, and `Error`.

### Changed

- Moved middleware primitives into core for reuse by higher-level middleware modules.
- Improved HTTP cache integration points and route middleware flow.

## [1.6.2] - 2025-12-11

### Changed

- Refined the HTTP facade, handler system, and configuration loading.
- Exposed public request/response types through umbrella headers.
- Improved `RequestHandler` concepts and support for params, query values, and JSON request bodies.
- Made configuration discovery more deterministic across relative, absolute, and project-root paths.

## [0.1.3] - 2025-10-06

### Added

- Added modular C++ framework structure with `core`, `orm`, `cli`, `docs`, `middleware`, `websocket`, devtools, and examples.
- Added `App` for simplified HTTP server setup.
- Added router support for dynamic route parameters.
- Added JSON response helpers and basic middleware support.

### Changed

- Integrated configurable logging through `spdlog`.
- Improved request parameter extraction performance.

### Fixed

- Fixed path parameter extraction for `string_view` inputs.
- Fixed default unmatched-route JSON response behavior.

## [0.1.0] - 2025-10-06

### Added

- Initial core module release with HTTP server, routing, and request handlers.
- Added simple JSON and text response examples.
- Added thread-safe server shutdown handling.
- Added performance measurement script integration.

### Changed

- Optimized route parameter parsing to avoid `boost::regex` overhead.

### Fixed

- Fixed request handler compilation errors related to `string_view` mismatches.
- Fixed minor app initialization and signal handling issues.

## [0.0.1] - Draft

### Added

- Created the initial project skeleton, CMake setup, and placeholder module layout.
