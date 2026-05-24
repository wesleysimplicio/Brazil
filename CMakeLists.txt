cmake_minimum_required(VERSION 3.27)

project(us4_v6_simplicio_apple
  VERSION 0.1.0
  DESCRIPTION "US4 V6 - Universal State Runtime for local LLM inference (Apple edition)"
  LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(US4_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(runtime)
add_subdirectory(apps)

if(US4_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
