FetchContent_Declare(
  Catch2
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.2.tar.gz
  URL_HASH SHA256=269543a49eb76f40b3f93ff231d4c24c27a7e16c90e47d2e45bcc564de470c6e
)
FetchContent_MakeAvailable(Catch2)

add_executable(absmartly_tests
  murmur3_test.cpp
  md5_test.cpp
  variant_assigner_test.cpp
  utils_test.cpp
  json_expr_test.cpp
  operators_test.cpp
  audience_matcher_test.cpp
  context_test.cpp
  system_attributes_test.cpp
  assignment_rules_test.cpp
  holdouts_test.cpp
  switchback_test.cpp
  context_switchback_test.cpp
  switchback_cluster_test.cpp
  switchback_client_test.cpp
  http_integration_test.cpp
)

target_link_libraries(absmartly_tests PRIVATE absmartly-sdk Catch2::Catch2WithMain)
# Lets tests check the generated SDK version against the single source of truth.
target_compile_definitions(absmartly_tests PRIVATE ABSMARTLY_PROJECT_VERSION="${PROJECT_VERSION}")

include(CTest)
include(Catch)
catch_discover_tests(absmartly_tests)
target_compile_definitions(absmartly_tests PRIVATE ABSMARTLY_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# Public headers stay clean under strict consumer warnings.
add_library(absmartly_public_headers_check OBJECT public_headers_check.cpp)
target_link_libraries(absmartly_public_headers_check PRIVATE absmartly-sdk)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(absmartly_public_headers_check PRIVATE -Wfloat-equal -Werror)
endif()
