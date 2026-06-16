#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BENCH_VERSION="${VIX_BENCH_VERSION:-dev}"
BENCH_RUNNER="${VIX_BENCH_RUNNER:-manual}"
BENCH_MACHINE="${VIX_BENCH_MACHINE:-local}"

OUT_DIR="${VIX_BENCH_OUT_DIR:-${CORE_ROOT}/benchmarks/results/${BENCH_VERSION}}"
BIN_DIR="${VIX_CORE_BENCH_BIN_DIR:-}"

BENCHMARKS=(
  core_runtime_task_bench
  core_runtime_queue_bench
  core_runtime_scheduler_bench
  core_runtime_worker_bench

  core_executor_submit_bench
  core_executor_post_bench
  core_executor_metrics_bench

  core_router_match_bench
  core_router_registration_bench

  core_http_request_bench
  core_http_response_bench

  core_session_fake_transport_bench

  core_app_route_registration_bench
  core_app_group_registration_bench
)

usage() {
  cat <<EOF
Usage:
  scripts/run_core_benchmarks.sh [options]

Options:
  --bin-dir DIR      Directory containing benchmark binaries
  --out-dir DIR      Directory where JSON results will be written
  --version VERSION  Benchmark version label, for example v2.6.3
  --runner NAME      Runner name, for example official or local
  --machine NAME     Machine name, for example vix-bench-runner-1
  -h, --help         Show this help

Environment:
  VIX_CORE_BENCH_BIN_DIR
  VIX_BENCH_OUT_DIR
  VIX_BENCH_VERSION
  VIX_BENCH_RUNNER
  VIX_BENCH_MACHINE
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bin-dir)
      BIN_DIR="${2:?missing value for --bin-dir}"
      shift 2
      ;;

    --out-dir)
      OUT_DIR="${2:?missing value for --out-dir}"
      shift 2
      ;;

    --version)
      BENCH_VERSION="${2:?missing value for --version}"
      shift 2
      ;;

    --runner)
      BENCH_RUNNER="${2:?missing value for --runner}"
      shift 2
      ;;

    --machine)
      BENCH_MACHINE="${2:?missing value for --machine}"
      shift 2
      ;;

    -h|--help)
      usage
      exit 0
      ;;

    *)
      echo "[core/benchmarks] unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

find_bench_bin_dir() {
  local candidate

  for candidate in \
    "${CORE_ROOT}/build/benchmarks/core" \
    "${CORE_ROOT}/build-ninja/benchmarks/core" \
    "${CORE_ROOT}/build-release/benchmarks/core" \
    "${CORE_ROOT}/build-debug/benchmarks/core" \
    "${CORE_ROOT}/cmake-build-release/benchmarks/core" \
    "${CORE_ROOT}/cmake-build-debug/benchmarks/core"
  do
    if [[ -x "${candidate}/core_runtime_task_bench" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  candidate="$(
    find "${CORE_ROOT}" \
      -type f \
      -name core_runtime_task_bench \
      -path '*/benchmarks/core/*' \
      -print \
      -quit 2>/dev/null || true
  )"

  if [[ -n "${candidate}" ]]; then
    dirname "${candidate}"
    return 0
  fi

  return 1
}

if [[ -z "${BIN_DIR}" ]]; then
  if ! BIN_DIR="$(find_bench_bin_dir)"; then
    echo "[core/benchmarks] benchmark binaries were not found." >&2
    echo "[core/benchmarks] build them first with:" >&2
    echo "  vix build --build-target core_benchmarks -- -DVIX_CORE_BUILD_BENCHMARKS=ON" >&2
    exit 1
  fi
fi

if [[ ! -d "${BIN_DIR}" ]]; then
  echo "[core/benchmarks] binary directory does not exist: ${BIN_DIR}" >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

export VIX_BENCH_VERSION="${BENCH_VERSION}"
export VIX_BENCH_RUNNER="${BENCH_RUNNER}"
export VIX_BENCH_MACHINE="${BENCH_MACHINE}"

export VIX_ENV_SILENT="${VIX_ENV_SILENT:-true}"
export VIX_DOCS="${VIX_DOCS:-false}"
export VIX_ACCESS_LOGS="${VIX_ACCESS_LOGS:-false}"
export VIX_INTERNAL_LOGS="${VIX_INTERNAL_LOGS:-false}"
export VIX_LOG_ASYNC="${VIX_LOG_ASYNC:-false}"
export VIX_LOG_LEVEL="${VIX_LOG_LEVEL:-critical}"

echo "[core/benchmarks] runner:  ${VIX_BENCH_RUNNER}"
echo "[core/benchmarks] machine: ${VIX_BENCH_MACHINE}"
echo "[core/benchmarks] version: ${VIX_BENCH_VERSION}"
echo "[core/benchmarks] bin dir: ${BIN_DIR}"
echo "[core/benchmarks] out dir: ${OUT_DIR}"
echo

FAILED=0

for bench in "${BENCHMARKS[@]}"; do
  binary="${BIN_DIR}/${bench}"
  output="${OUT_DIR}/${bench}.json"

  if [[ ! -x "${binary}" ]]; then
    echo "[core/benchmarks] missing benchmark binary: ${binary}" >&2
    FAILED=1
    continue
  fi

  echo "[core/benchmarks] running ${bench}"

  if "${binary}" "${output}"; then
    echo "[core/benchmarks] wrote ${output}"
  else
    echo "[core/benchmarks] failed: ${bench}" >&2
    FAILED=1
  fi

  echo
done

if [[ "${FAILED}" -ne 0 ]]; then
  echo "[core/benchmarks] one or more benchmarks failed." >&2
  exit 1
fi

echo "[core/benchmarks] done."
echo "[core/benchmarks] results: ${OUT_DIR}"
