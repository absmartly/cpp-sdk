# Builds a program against an installed/extracted SDK package via find_package,
# to check the exported targets, headers and version work outside the source tree.
cmake_minimum_required(VERSION 3.16)
project(absmartly_package_consumer LANGUAGES CXX)

find_package(absmartly-sdk REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE absmartly::absmartly-sdk)
