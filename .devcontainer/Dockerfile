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

# --- ADDED FOR DEPLOYMENT ---

# Keep the exact same path so your C++ read_file() absolute paths still work
WORKDIR /workspaces/PixelCanvas

# Copy all your project files into the container
COPY . .

# Build the C++ project
RUN cmake -B build -S . && cmake --build build

# Start the server on port 8080 (Nginx will route to this)
CMD ["./build/pixel_server", "8080"]