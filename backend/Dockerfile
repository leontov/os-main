# syntax=docker/dockerfile:1.5

FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        ninja-build \
        pkg-config \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt ./
COPY backend ./backend
COPY apps ./apps

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DKOLIBRI_ENABLE_TESTS=OFF \
    && cmake --build build --target kolibri_node

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /app/build/kolibri_node /usr/local/bin/kolibri_node

EXPOSE 4050

ENTRYPOINT ["/usr/local/bin/kolibri_node"]
CMD ["--listen", "4050"]
