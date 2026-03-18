# CMake configuration for the PixelCanvas backend
cmake_minimum_required(VERSION 3.20)
project(PixelCanvas)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)

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
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE) # Skip tests for performance
FetchContent_MakeAvailable(redis_pp)

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
pkg_check_modules(PQXX REQUIRED libpqxx) # libpqxx for PostgreSQL interaction
pkg_check_modules(SODIUM REQUIRED libsodium) # libsodium for password hashing

# --- TARGET CONFIGURATION ---
add_executable(pixel_server src/main.cpp)

# Link all dependencies to the main executable
target_link_libraries(pixel_server 
    PRIVATE 
    Crow::Crow
    redis++
    nlohmann_json::nlohmann_json
    jwt-cpp::jwt-cpp
    OpenSSL::Crypto
    ${PQXX_LIBRARIES}
    ${SODIUM_LIBRARIES}
)

# Set include directories for linked libraries
target_include_directories(pixel_server PRIVATE 
    ${PQXX_INCLUDE_DIRS} 
    ${OPENSSL_INCLUDE_DIR}
    ${SODIUM_INCLUDE_DIRS}
)
