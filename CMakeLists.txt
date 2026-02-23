cmake_minimum_required(VERSION 3.20)
project(KhorNameShieldService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# ---------- Dependencies ----------
# nlohmann/json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Crow (maintained)
FetchContent_Declare(
  crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG        v1.2.0
)
FetchContent_MakeAvailable(crow)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# ---------- Target ----------
add_executable(KhorNameShieldService
  src/main.cpp
)

# Crow include style differs by version; CrowCpp/Crow exports Crow::Crow
target_link_libraries(KhorNameShieldService PRIVATE
  Crow::Crow
  nlohmann_json::nlohmann_json
  Threads::Threads
  OpenSSL::Crypto
)

# Optional: nicer warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(KhorNameShieldService PRIVATE -Wall -Wextra -Wpedantic)
endif()