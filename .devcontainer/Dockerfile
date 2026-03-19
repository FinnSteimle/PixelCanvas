FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    software-properties-common \
    gpg \
    ca-certificates \
    curl \
    tar \
    && add-apt-repository ppa:ubuntu-toolchain-r/test -y \
    && apt-get update \
    && apt-get -y install --no-install-recommends \
    gcc-15 \
    g++-15 \
    build-essential \
    cmake \
    git \
    libpq-dev \
    libhiredis-dev \
    libasio-dev \
    libssl-dev \
    libsodium-dev \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 100 --slave /usr/bin/g++ g++ /usr/bin/g++-15

RUN curl -L https://github.com/grafana/k6/releases/download/v0.50.0/k6-v0.50.0-linux-amd64.tar.gz -o k6.tar.gz \
    && tar -xzf k6.tar.gz \
    && mv k6-v0.50.0-linux-amd64/k6 /usr/local/bin/k6 \
    && rm -rf k6.tar.gz k6-v0.50.0-linux-amd64