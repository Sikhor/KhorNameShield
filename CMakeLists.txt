cmake_minimum_required(VERSION 3.20)
project(KhorNameShieldService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# nlohmann/json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Crow (use a maintained fork; original is sometimes stale)
FetchContent_Declare(
  crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG        v1.2.0
)
FetchContent_MakeAvailable(crow)

add_executable(KhorNameShield main.cpp)

# Crow target name can differ by version; this works with CrowCpp/Crow.
target_link_libraries(KhorNameShield PRIVATE Crow::Crow nlohmann_json::nlohmann_json)

# pthread on Linux (some setups need it explicitly)
find_package(Threads REQUIRED)
target_link_libraries(KhorNameShield PRIVATE Threads::Threads)