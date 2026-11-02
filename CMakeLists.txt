cmake_minimum_required(VERSION 3.16)
project(absmartly-sdk VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(json)

add_library(absmartly-sdk
  src/curl_global.cpp
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

target_compile_features(absmartly-sdk PUBLIC cxx_std_17)
target_link_libraries(absmartly-sdk PUBLIC nlohmann_json::nlohmann_json)

find_package(CURL REQUIRED)
target_sources(absmartly-sdk PRIVATE src/default_http_client.cpp src/async_http_client.cpp)
target_link_libraries(absmartly-sdk PUBLIC CURL::libcurl)

set(ABSMARTLY_BUILD_TESTS_DEFAULT OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(ABSMARTLY_BUILD_TESTS_DEFAULT ON)
endif()
option(ABSMARTLY_BUILD_TESTS "Build tests" ${ABSMARTLY_BUILD_TESTS_DEFAULT})
if(ABSMARTLY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
