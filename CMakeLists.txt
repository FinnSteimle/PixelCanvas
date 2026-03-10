cmake_minimum_required(VERSION 3.20)
project(PixelCanvas)

set(CMAKE_CXX_STANDARD 17)


include(FetchContent)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(crow)

FetchContent_Declare(
    redis_pp
    GIT_REPOSITORY https://github.com/sewenew/redis-plus-plus.git
    GIT_TAG 1.3.10
)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.6.0
)
FetchContent_MakeAvailable(jwt-cpp)

set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(redis_pp)

find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)


add_executable(pixel_server src/main.cpp)


target_link_libraries(pixel_server 
    PRIVATE 
    Crow::Crow
    redis++
    nlohmann_json::nlohmann_json
    jwt-cpp::jwt-cpp
    OpenSSL::Crypto
    ${PQXX_LIBRARIES}
)

# Tell the compiler where the headers are
target_include_directories(pixel_server PRIVATE ${PQXX_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
