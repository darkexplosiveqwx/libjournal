#!/usr/bin/env bash
set -Eeuo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly project_dir
readonly build_dir="$project_dir/build"
readonly demo="$project_dir/journal-demo"

command_name=build
build_type=Release
static=false
clean_first=false
container_test=false

usage() {
  cat <<'EOF'
Usage: ./build.sh [command] [options]

Commands:
  build                 Build the library, demo, and tests (default)
  test                  Build and run the tests locally
  container             Build the static demo with Podman or Docker
  clean                 Remove local build artifacts

Options:
  --debug               Build with debug symbols and checks
  --release             Build an optimized release (default)
  --static              Build a fully static binary with musl-gcc
  --clean               Remove existing artifacts before building
  -h, --help            Show this help

Container tests:
  ./build.sh container test

Examples:
  ./build.sh
  ./build.sh build --debug
  ./build.sh test --clean
  ./build.sh container --release
EOF
}

die() {
  echo "error: $*" >&2
  exit 2
}

log() {
  printf '\n==> %s\n' "$*"
}

clean_artifacts() {
  log "Cleaning build artifacts"
  rm -rf -- "$build_dir" "$demo"
}

find_container_engine() {
  if command -v podman >/dev/null 2>&1; then
    command -v podman
  elif command -v docker >/dev/null 2>&1; then
    command -v docker
  else
    die "container command requires podman or docker"
  fi
}

configure() {
  local -a cmake_args=(-S "$project_dir" -B "$build_dir" "-DCMAKE_BUILD_TYPE=$build_type")

  if $static; then
    command -v musl-gcc >/dev/null 2>&1 || die "--static requires musl-gcc"
    local static_inc
    static_inc=$(mktemp -d)
    for dir in linux asm-generic asm; do
      ln -sf "/usr/include/$dir" "$static_inc/$dir"
    done
    cmake_args+=(-DBUILD_SHARED_LIBS=OFF -DCMAKE_EXE_LINKER_FLAGS=-static "-DCMAKE_C_FLAGS=-isystem $static_inc")
    if ! CC=musl-gcc cmake "${cmake_args[@]}"; then
      rm -rf -- "$static_inc"
      return 1
    fi
    rm -rf -- "$static_inc"
  else
    cmake "${cmake_args[@]}"
  fi
}

build_local() {
  local variant=""
  $static && variant=" (static)"
  log "Configuring $build_type build$variant"
  configure
  log "Building library, demo, and tests"
  cmake --build "$build_dir" --target journal-demo test-journal
  cp -- "$build_dir/src/demo/journal-demo" "$demo"
  echo "built: $demo"
}

run_tests() {
  build_local
  log "Running tests"
  ctest --test-dir "$build_dir" --output-on-failure
}

build_container() {
  local engine
  engine=$(find_container_engine)
  local image=libjournal:latest
  local container_id

  log "Building static container image with $(basename "$engine")"
  if [[ $build_type == Debug ]]; then
    "$engine" build -t "$image" -f "$project_dir/container/Containerfile" --build-arg buildtype=Debug "$project_dir"
  else
    "$engine" build -t "$image" -f "$project_dir/container/Containerfile" "$project_dir"
  fi

  container_id=$("$engine" create "$image")
  "$engine" cp "$container_id:/journal-demo" "$demo"
  "$engine" rm "$container_id" >/dev/null 2>&1 || true
  echo "built: $demo"
}

run_container_tests() {
  local engine
  engine=$(find_container_engine)
  local image=libjournal-test:latest

  log "Building and running container tests with $(basename "$engine")"
  "$engine" build --target test -t "$image" -f "$project_dir/container/Containerfile" "$project_dir"
  exec "$engine" run --rm -t "$image"
}

parse_args() {
  local arg
  while (($#)); do
    arg=$1
    case $arg in
      build|test|container|clean)
        if [[ $arg == test && $command_name == container ]]; then
          container_test=true
          shift
          continue
        fi
        [[ $command_name == build ]] || die "multiple commands specified"
        command_name=$arg
        ;;
      debug|--debug) build_type=Debug ;;
      release|--release) build_type=Release ;;
      static|--static) static=true ;;
      --clean) clean_first=true ;;
      -h|--help) usage; exit 0 ;;
      container-test) command_name=container; container_test=true ;;
      *) die "unknown argument '$arg' (use --help for usage)" ;;
    esac
    shift
  done
}

parse_args "$@"

if [[ $command_name == clean ]]; then
  clean_artifacts
  exit 0
fi

if $clean_first; then
  clean_artifacts
fi

case $command_name in
  build) build_local ;;
  test) run_tests ;;
  container)
    if $container_test; then
      run_container_tests
    else
      build_container
    fi
    ;;
esac
