# Vix Core Benchmarks

This directory contains the official benchmark suite for the Vix.cpp Core module.

The benchmarks are used to track performance across releases and to catch regressions before a change is merged or released.

## Layout

```txt
benchmarks/
├── CMakeLists.txt
├── README.md
├── common/
│   ├── Benchmark.hpp
│   └── BenchmarkJson.hpp
├── runtime/
├── executor/
├── router/
├── http/
├── session/
├── app/
├── baselines/
└── results/
```

## Benchmark groups

The suite currently covers:

```txt
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

## Build mode

Benchmarks must be built in release mode.

Do not use a dev/debug build for official numbers.

```txt
dev / dev-ninja  = compile, debug, validate
release          = measure performance and create official baselines
```

A dev build is useful to confirm that the benchmark sources compile, but the numbers are not stable enough to be used as release data.

## Build benchmarks

From `modules/core`:

```bash
vix build --clean --preset release --build-target all -v -- \
  -DVIX_CORE_BUILD_BENCHMARKS=ON \
  -DVIX_CORE_BUILD_TESTS=ON
```

Benchmark binaries are generated under:

```txt
build-release/benchmarks/core/
```

List them with:

```bash
ls -lah build-release/benchmarks/core
```

## Run one benchmark

Example:

```bash
./build-release/benchmarks/core/core_router_match_bench
```

Write JSON output:

```bash
./build-release/benchmarks/core/core_router_match_bench \
  benchmarks/results/dev/core_router_match_bench.json
```

## Run the full benchmark suite

```bash
./scripts/run_core_benchmarks.sh \
  --bin-dir build-release/benchmarks/core \
  --out-dir benchmarks/results/dev \
  --version dev \
  --runner local \
  --machine gaspard-pc
```

The script runs every benchmark binary and writes one JSON report per benchmark executable.

Example output directory:

```txt
benchmarks/results/dev/
```

## Create an official baseline

Official baselines are stored under `benchmarks/baselines/<version>`.

Example for `v2.6.3`:

```bash
./scripts/run_core_benchmarks.sh \
  --bin-dir build-release/benchmarks/core \
  --out-dir benchmarks/baselines/v2.6.3 \
  --version v2.6.3 \
  --runner official \
  --machine vix-bench-runner-1
```

This creates files such as:

```txt
benchmarks/baselines/v2.6.3/core_runtime_task_bench.json
benchmarks/baselines/v2.6.3/core_runtime_queue_bench.json
benchmarks/baselines/v2.6.3/core_runtime_scheduler_bench.json
benchmarks/baselines/v2.6.3/core_runtime_worker_bench.json
benchmarks/baselines/v2.6.3/core_executor_submit_bench.json
benchmarks/baselines/v2.6.3/core_executor_post_bench.json
benchmarks/baselines/v2.6.3/core_executor_metrics_bench.json
benchmarks/baselines/v2.6.3/core_router_match_bench.json
benchmarks/baselines/v2.6.3/core_router_registration_bench.json
benchmarks/baselines/v2.6.3/core_http_request_bench.json
benchmarks/baselines/v2.6.3/core_http_response_bench.json
benchmarks/baselines/v2.6.3/core_session_fake_transport_bench.json
benchmarks/baselines/v2.6.3/core_app_route_registration_bench.json
benchmarks/baselines/v2.6.3/core_app_group_registration_bench.json
```

## Compare against a baseline

Run a new benchmark set:

```bash
./scripts/run_core_benchmarks.sh \
  --bin-dir build-release/benchmarks/core \
  --out-dir benchmarks/results/current \
  --version current \
  --runner local \
  --machine gaspard-pc
```

Compare it against the official baseline:

```bash
./scripts/compare_core_benchmarks.py \
  benchmarks/baselines/v2.6.3 \
  benchmarks/results/current
```

Default comparison metric:

```txt
median_ops_per_sec
```

By default:

```txt
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

Write comparison JSON:

```bash
./scripts/compare_core_benchmarks.py \
  benchmarks/baselines/v2.6.3 \
  benchmarks/results/current \
  --json-out benchmarks/results/current/compare-v2.6.3.json
```

## Result format

Each benchmark executable writes a JSON report with:

```json
{
  "suite": "vix.core.router.match",
  "version": "v2.6.3",
  "results": [
    {
      "name": "router.match/static_route",
      "operations": 1000000,
      "median_ms": 106.674,
      "mean_ms": 106.545,
      "median_ops_per_sec": 9374362.37,
      "mean_ops_per_sec": 9385700.0
    }
  ]
}
```

The most important metric for regression checks is:

```txt
median_ops_per_sec
```

Higher is better.

## Recommended workflow

Before changing performance-sensitive code:

```bash
./scripts/run_core_benchmarks.sh \
  --bin-dir build-release/benchmarks/core \
  --out-dir benchmarks/results/before \
  --version before \
  --runner local \
  --machine gaspard-pc
```

After the change:

```bash
./scripts/run_core_benchmarks.sh \
  --bin-dir build-release/benchmarks/core \
  --out-dir benchmarks/results/after \
  --version after \
  --runner local \
  --machine gaspard-pc
```

Compare:

```bash
./scripts/compare_core_benchmarks.py \
  benchmarks/results/before \
  benchmarks/results/after \
  --allow-new \
  --allow-missing
```

Before a release, compare against the official baseline:

```bash
./scripts/compare_core_benchmarks.py \
  benchmarks/baselines/v2.6.3 \
  benchmarks/results/after
```

## Rules for official benchmark runs

Use the same machine when possible.

Use release builds only.

Avoid running heavy background tasks during benchmark runs.

Keep the benchmark runner name and machine name stable.

Use versioned baseline directories.

Do not replace an official baseline unless the release version or benchmark suite intentionally changed.

## Notes

Some very small benchmarks can report extremely small median times. In those cases, `ops/sec` can look very large. These benchmarks are still useful as guardrails, but they should be interpreted together with larger workload benchmarks.

For stable release decisions, prefer the broader benchmark groups:

```txt
runtime.scheduler
runtime.worker
executor.submit
executor.post
router.match
router.registration
http.request
http.response
session.fake_transport
app.route_registration
app.group_registration
```
