# CMake configuration for the PixelCanvas backend
cmake_minimum_required(VERSION 3.20)

# Explicitly set GCC 15 before project initialization
set(CMAKE_C_COMPILER gcc-15)
set(CMAKE_CXX_COMPILER g++-15)

project(PixelCanvas)

# Use C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Suppress ABI notes/warnings from third-party headers
add_compile_options(-Wno-psabi)

include(FetchContent)

# --- DEPENDENCIES ---

# Crow: C++ microframework for web and WebSockets
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.3.1
)
FetchContent_MakeAvailable(crow)

# Redis Plus Plus: C++ client for Redis
FetchContent_Declare(
    redis_pp
    GIT_REPOSITORY https://github.com/sewenew/redis-plus-plus.git
    GIT_TAG 1.3.15
)
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(redis_pp)

# libpqxx: C++ client for PostgreSQL (Built from source to ensure GCC 15/C++20 ABI compatibility)
FetchContent_Declare(
    libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG 7.9.1
)
set(SKIP_PQXX_STATIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libpqxx)

# JSON for Modern C++ (nlohmann/json)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# JWT-CPP: For JSON Web Token creation and validation
set(JWT_CPP_EXTERNAL_NLOHMANN_JSON ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.2
)
FetchContent_MakeAvailable(jwt-cpp)

# --- SYSTEM PACKAGES ---
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)

# --- TARGET CONFIGURATION ---
add_executable(pixel_server 
    src/main.cpp 
    src/DBManager.cpp 
    src/RedisManager.cpp
)

# Link all dependencies to the main executable
target_link_libraries(pixel_server 
    PRIVATE 
    Crow::Crow
    redis++
    pqxx
    nlohmann_json::nlohmann_json
    jwt-cpp::jwt-cpp
    OpenSSL::Crypto
    ${SODIUM_LIBRARIES}
)

# Set include directories for linked libraries
target_include_directories(pixel_server PRIVATE 
    ${OPENSSL_INCLUDE_DIR}
    ${SODIUM_INCLUDE_DIRS}
)