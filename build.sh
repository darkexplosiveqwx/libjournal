#!/usr/bin/env bash
set -euo pipefail

clean=false
nobuild=false
static=false
container=false
test_container=false
debug=false
release=false
for arg in "$@"; do
  [ "$arg" = "clean" ] && clean=true
  [ "$arg" = "nobuild" ] && nobuild=true
  [ "$arg" = "static" ] && static=true
  [ "$arg" = "container" ] && container=true
  [ "$arg" = "test" ] && test_container=true
  [ "$arg" = "debug" ] && debug=true
  [ "$arg" = "release" ] && release=true
done

if $debug && $release; then
  echo "error: debug and release are mutually exclusive" >&2
  exit 1
fi

$clean && rm -rf build journal-demo

buildtype=
$debug && buildtype="Debug"
$release && buildtype="Release"

if $container; then
  image="libjournal:latest"
  buildctl=$(command -v podman || command -v docker)
  if [ -z "$buildctl" ]; then
    echo "error: neither podman nor docker found" >&2
    exit 1
  fi
  $buildctl build -t "$image" -f container/Containerfile ${buildtype:+--build-arg buildtype=$buildtype} .
  cid=$($buildctl create "$image")
  $buildctl cp "$cid":/journal-demo .
  $buildctl rm "$cid" >/dev/null
  echo "built: $(pwd)/journal-demo"
  exit 0
fi

if $test_container; then
  rm -rf build journal-demo
  image="libjournal-test:latest"
  buildctl=$(command -v podman || command -v docker)
  if [ -z "$buildctl" ]; then
    echo "error: neither podman nor docker found" >&2
    exit 1
  fi
  $buildctl build --target test -t "$image" -f container/Containerfile .
  exec $buildctl run --rm -t "$image"
fi

$nobuild && exit 0

static_inc=
if $static; then
  static_inc=$(mktemp -d)
  for dir in linux asm-generic asm; do
    ln -sf "/usr/include/$dir" "$static_inc/$dir"
  done
fi

cmake_args=(-S . -B build)
if $static; then
  cmake_args+=(-DBUILD_SHARED_LIBS=OFF -DCMAKE_EXE_LINKER_FLAGS=-static "-DCMAKE_C_FLAGS=-isystem $static_inc")
fi
cmake_args+=(${buildtype:+-DCMAKE_BUILD_TYPE=$buildtype})

if $static; then
  CC=musl-gcc cmake "${cmake_args[@]}"
else
  cmake "${cmake_args[@]}"
fi
cmake --build build --target journal-demo test-journal
cp build/src/demo/journal-demo .
if [ -n "$static_inc" ]; then
  rm -rf "$static_inc"
fi
