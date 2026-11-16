# Embeds the SDK with add_subdirectory, as a FetchContent/add_subdirectory user
# does, with its own install() and CPack. The SDK must not add its install or
# package rules to the embedding project's.
cmake_minimum_required(VERSION 3.16)
project(absmartly_subdirectory_consumer VERSION 2.3.4 LANGUAGES CXX)

set(CPACK_PACKAGE_NAME "embedding-app")
set(CPACK_GENERATOR "TGZ")
include(CPack)

add_subdirectory(${ABSMARTLY_SDK_SOURCE_DIR} absmartly-sdk)

add_executable(app ../package_consumer/main.cpp)
target_link_libraries(app PRIVATE absmartly::sdk)
install(TARGETS app RUNTIME DESTINATION bin)
