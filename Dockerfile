# ── Stage 1: Build ──────────────────────────────────────────
FROM gcc:14-bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build pkg-config libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt CMakePresets.json vcpkg.json ./
COPY src/    src/
COPY include/ include/
COPY tests/  tests/

RUN cmake --preset debug && cmake --build --preset debug

# ── Stage 2: Runtime ───────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libsqlite3-0 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/debug/cpp-rest-api /usr/local/bin/cpp-rest-api

EXPOSE 8080
VOLUME ["/data"]

ENTRYPOINT ["cpp-rest-api", "--port", "8080", "--db", "/data/items.db"]
