FROM alpine:latest AS build

ARG buildtype=""

RUN apk add --no-cache \
    build-base \
    cmake \
    linux-headers

COPY . /src
WORKDIR /src

RUN cmake -S . -B build \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_EXE_LINKER_FLAGS=-static \
      ${buildtype:+-DCMAKE_BUILD_TYPE=$buildtype} && \
    cmake --build build --target journal-demo && \
    cp build/src/demo/journal-demo /journal-demo

FROM scratch
COPY --from=build /journal-demo /journal-demo
