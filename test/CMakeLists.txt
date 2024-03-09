# Copyright (c) 2024 Pyarelal Knowles, MIT License

cmake_minimum_required(VERSION 3.20)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(googletest)
endif()

if(NOT TARGET decodeless::allocator)
  if(DECODELESS_SEARCH_DEPENDENCIES)
    find_package(decodeless_allocator REQUIRED CONFIG PATHS "${CMAKE_CURRENT_LIST_DIR}/../../allocator")
  elseif(DECODELESS_FETCH_DEPENDENCIES)
    include(FetchContent)
    FetchContent_Declare(
      decodeless_allocator
      GIT_REPOSITORY https://github.com/decodeless/allocator.git
      GIT_TAG main)
    FetchContent_MakeAvailable(decodeless_allocator)
  else()
    message(
      FATAL_ERROR
        "Dependency decodeless::allocator not found! You can enable searching with DECODELESS_SEARCH_DEPENDENCIES or downloading with DECODELESS_FETCH_DEPENDENCIES"
    )
  endif()
endif()

# Unit tests
add_executable(${PROJECT_NAME}_tests src/header.cpp)
target_link_libraries(
  ${PROJECT_NAME}_tests
  decodeless::header
  decodeless::allocator
  gtest_main
  gmock_main)

# TODO: presets?
# https://stackoverflow.com/questions/45955272/modern-way-to-set-compiler-flags-in-cross-platform-cmake-project
if(MSVC)
  target_compile_options(${PROJECT_NAME}_tests PRIVATE /W4 /WX)
  target_compile_definitions(${PROJECT_NAME}_tests PRIVATE WIN32_LEAN_AND_MEAN=1 NOMINMAX)
else()
  target_compile_options(${PROJECT_NAME}_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME}_tests PRIVATE -fbounds-check)
  endif()
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)
