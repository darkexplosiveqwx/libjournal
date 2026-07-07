#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" = "clean" ]; then
  rm -rf build journal-demo
  exit 0
fi

cmake -S . -B build
cmake --build build --target journal-demo
cp build/src/demo/journal-demo .
