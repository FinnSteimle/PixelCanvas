cmake_minimum_required(VERSION 3.20)
project(PixelCanvas)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.3.1
)
FetchContent_MakeAvailable(crow)

FetchContent_Declare(
    redis_pp
    GIT_REPOSITORY https://github.com/sewenew/redis-plus-plus.git
    GIT_TAG 1.3.15
)
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(redis_pp)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

set(JWT_CPP_EXTERNAL_NLOHMANN_JSON ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.2
)
FetchContent_MakeAvailable(jwt-cpp)

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
pkg_check_modules(SODIUM REQUIRED libsodium)

add_executable(pixel_server src/main.cpp)

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

target_include_directories(pixel_server PRIVATE 
    ${PQXX_INCLUDE_DIRS} 
    ${OPENSSL_INCLUDE_DIR}
    ${SODIUM_INCLUDE_DIRS}
)
