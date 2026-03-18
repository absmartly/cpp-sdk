cmake_minimum_required(VERSION 3.16)
project(absmartly-sdk VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_library(absmartly-sdk
  src/md5.cpp
  src/murmur3.cpp
  src/variant_assigner.cpp
  src/utils.cpp
  src/context.cpp
  src/client.cpp
  src/default_context_data_provider.cpp
  src/default_context_publisher.cpp
  src/sdk.cpp
  src/json_expr/evaluator.cpp
  src/json_expr/operators.cpp
  src/audience_matcher.cpp
)

target_include_directories(absmartly-sdk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(absmartly-sdk PUBLIC nlohmann_json::nlohmann_json)

find_package(CURL REQUIRED)
target_sources(absmartly-sdk PRIVATE src/default_http_client.cpp src/async_http_client.cpp)
target_link_libraries(absmartly-sdk PUBLIC CURL::libcurl)

option(ABSMARTLY_BUILD_TESTS "Build tests" ON)
if(ABSMARTLY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
