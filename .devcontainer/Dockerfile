FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    build-essential \
    cmake \
    git \
    libpq-dev \
    libpqxx-dev \
    libhiredis-dev \
    libasio-dev \
    libssl-dev \
    libsodium-dev \
    gpg \
    ca-certificates

RUN apt-get update && apt-get install -y curl tar \
    && curl -L https://github.com/grafana/k6/releases/download/v0.50.0/k6-v0.50.0-linux-amd64.tar.gz -o k6.tar.gz \
    && tar -xzf k6.tar.gz \
    && mv k6-v0.50.0-linux-amd64/k6 /usr/local/bin/k6 \
    && rm -rf k6.tar.gz k6-v0.50.0-linux-amd64