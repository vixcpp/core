# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)  
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]
## [1.13.1] - 2026-01-02

### Added
- 

### Changed
- 

### Removed
- 

## [1.13.0] - 2026-01-02

### Added
- 

### Changed
- 

### Removed
- 

## [1.12.0] - 2025-12-25

### Added
- 

### Changed
- 

### Removed
- 

## [1.11.0] - 2025-12-24

### Added
- 

### Changed
- 

### Removed
- 


### Core v1.11.0

This release introduces foundational middleware primitives into the core
and significantly stabilizes the HTTP cache engine.

#### Middleware primitives (core)

- Added `core/mw/*` primitives:
  - `Context`
  - `Next` / `NextOnce`
  - `Hooks`
  - `Result` / `Error`
- These primitives are now owned by core and reused by middleware
- Eliminates circular dependency between core and middleware

#### HTTP cache engine improvements

- Refactored cache core:
  - `Cache`, `CacheEntry`, `CacheKey`, `CachePolicy`
  - `CacheContext` + `CacheContextMapper`
- Improved store implementations:
  - `MemoryStore`
  - `LruMemoryStore`
  - `FileStore`
- Better cache key stability and header handling
- Cleaner separation between policy, keying, and storage

#### App integration

- Updated `App` to support middleware-based cache usage
- Internal cleanup for cache/middleware interaction

#### Tests

- Updated cache smoke tests
- Added coverage for CacheContextMapper behavior

This release provides a clean, reusable middleware foundation inside core
and prepares the ecosystem for higher-level middleware APIs.

### Added

## [1.6.2] - 2025-12-11

core: refine HTTP facade, handler system and configuration loading

• Exposed clean public types (Request, Response) via umbrella headers (vix.hpp, core.h)
• Adjusted RequestHandler to: - fix header find() overloads - stabilize concepts (HandlerReqRes, HandlerReqResParams) - support new Request facade (params, query, JSON body)
• Improved App.cpp: - unified route registration logs - stronger error diagnostics in development mode - safer shutdown callback execution
• Reworked Config.cpp: - deterministic resolution of config/config.json - support for relative, absolute and project-root discovery - improved warnings and fallback behavior

-

### Changed

-

### Removed

-

## [0.1.3] - 2025-10-06

### Added

-

### Changed

-

### Removed

-

### Added

- Modular C++ framework structure with `core`, `orm`, `cli`, `docs`, `middleware`, `websocket`, `devtools`, `examples`.
- `App` class for simplified HTTP server setup.
- Router system supporting dynamic route parameters (`/users/{id}` style).
- JSON response wrapper using `nlohmann::json`.
- Middleware system for request handling.
- Example endpoints `/hello`, `/ping`, and `/users/{id}`.
- Thread-safe signal handling for graceful shutdown.
- Basic configuration system (`Config` class) to manage JSON config files.

### Changed

- Logger integrated using `spdlog` with configurable log levels.
- Improved request parameter extraction for performance.

### Fixed

- Path parameter extraction to correctly handle `string_view` types.
- Fixed default response for unmatched routes (`404` JSON message).

---

## [0.1.0] - 2025-10-06

### Added

- Initial release of core module with working HTTP server.
- Basic routing system and request handlers.
- Simple example endpoints demonstrating JSON and text responses.
- Thread-safe server shutdown handling.
- Integration of performance measurement scripts (FlameGraph ready).

### Changed

- Optimized route parameter parsing to avoid `boost::regex` overhead.

### Fixed

- Compilation errors due to `string_view` mismatch in request handler.
- Minor bug fixes in App initialization and signal handling.

---

## [0.0.1] - Draft

- Project skeleton created.
- Basic CMake setup and folder structure.
- Placeholder modules for `core`, `orm`, and `examples`.
