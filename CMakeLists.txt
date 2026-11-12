cmake_minimum_required(VERSION 3.16)
project(absmartly-sdk VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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

add_library(absmartly::sdk ALIAS absmartly-sdk)

target_include_directories(absmartly-sdk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(absmartly-sdk PUBLIC cxx_std_17)
target_link_libraries(absmartly-sdk PUBLIC
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
  $<INSTALL_INTERFACE:absmartly-sdk-json>
)

add_library(absmartly-sdk-json INTERFACE)
target_include_directories(absmartly-sdk-json INTERFACE
  $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

find_package(CURL REQUIRED)
target_sources(absmartly-sdk PRIVATE src/default_http_client.cpp src/async_http_client.cpp)
target_link_libraries(absmartly-sdk PUBLIC
  $<BUILD_INTERFACE:CURL::libcurl>
  $<INSTALL_INTERFACE:CURL::libcurl>
)

set(ABSMARTLY_BUILD_TESTS_DEFAULT OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(ABSMARTLY_BUILD_TESTS_DEFAULT ON)
endif()
option(ABSMARTLY_BUILD_TESTS "Build tests" ${ABSMARTLY_BUILD_TESTS_DEFAULT})
if(ABSMARTLY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(TARGETS absmartly-sdk absmartly-sdk-json
  EXPORT absmartly-sdk-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${json_SOURCE_DIR}/include/nlohmann DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(ABSMARTLY_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/absmartly-sdk)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/absmartly-sdkConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/absmartly-sdkConfig.cmake
  INSTALL_DESTINATION ${ABSMARTLY_CMAKE_INSTALL_DIR}
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/absmartly-sdkConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(EXPORT absmartly-sdk-targets
  FILE absmartly-sdk-targets.cmake
  NAMESPACE absmartly::
  DESTINATION ${ABSMARTLY_CMAKE_INSTALL_DIR}
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/absmartly-sdkConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/absmartly-sdkConfigVersion.cmake
  DESTINATION ${ABSMARTLY_CMAKE_INSTALL_DIR}
)

set(CPACK_PACKAGE_NAME "absmartly-cpp-sdk")
set(CPACK_PACKAGE_VENDOR "A/B Smartly")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A/B Smartly C++ SDK")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "TGZ;ZIP")
include(CPack)
