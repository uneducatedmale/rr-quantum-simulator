#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

need_file() {
  [[ -f "$1" ]] || fail "missing file: $1"
}

run_and_compare() {
  local name="$1"
  local expected="$2"
  shift 2

  need_file "$expected"

  local tmp
  tmp="$(mktemp)"
  if ! "$@" >"$tmp" 2>&1; then
    echo "---- output ($name) ----" >&2
    cat "$tmp" >&2 || true
    rm -f "$tmp"
    fail "command failed: $name"
  fi

  if ! diff -u "$expected" "$tmp" >/dev/null; then
    echo "---- diff ($name) ----" >&2
    diff -u "$expected" "$tmp" >&2 || true
    rm -f "$tmp"
    fail "output mismatch: $name"
  fi

  rm -f "$tmp"
}

run_expect_fail() {
  local name="$1"
  shift 1

  local tmp
  tmp="$(mktemp)"
  set +e
  "$@" >"$tmp" 2>&1
  local code=$?
  set -e

  if [[ "$code" -eq 0 ]]; then
    echo "---- output ($name) ----" >&2
    cat "$tmp" >&2 || true
    rm -f "$tmp"
    fail "expected failure (exit!=0): $name"
  fi

  rm -f "$tmp"
}

need_file "./rrsim"
need_file "./workloads/demo.txt"
need_file "./workloads/staggered.txt"

run_and_compare "single-demo-q2-cs1" "tests/expected/single-demo-q2-cs1.txt" \
  ./rrsim --input workloads/demo.txt --quantum 2 --cs 1

run_and_compare "single-staggered-q3-cs0" "tests/expected/single-staggered-q3-cs0.txt" \
  ./rrsim --input workloads/staggered.txt --quantum 3 --cs 0

run_and_compare "sweep-demo-1-6-cs1" "tests/expected/sweep-demo-1-6-cs1.txt" \
  ./rrsim --input workloads/demo.txt --sweep 1:6 --cs 1

run_and_compare "trace-demo-q2-cs1" "tests/expected/trace-demo-q2-cs1.txt" \
  ./rrsim --input workloads/demo.txt --quantum 2 --cs 1 --trace

run_expect_fail "invalid-flags-animate-trace" \
  ./rrsim --input workloads/demo.txt --quantum 2 --cs 1 --animate --trace

run_expect_fail "missing-input-file" \
  ./rrsim --input workloads/does-not-exist.txt --quantum 2 --cs 1

echo "OK"

