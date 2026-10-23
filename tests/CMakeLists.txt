FetchContent_Declare(
  Catch2
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.2.tar.gz
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
  http_integration_test.cpp
)

target_link_libraries(absmartly_tests PRIVATE absmartly-sdk Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(absmartly_tests)
