FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-22.04

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    build-essential \
    cmake \
    git \
    libpq-dev \
    libpqxx-dev \
    libhiredis-dev \
    libasio-dev

# Note: redis-plus-plus and Crow will be handled by CMake (FetchContent)