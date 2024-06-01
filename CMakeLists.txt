# Copyright (c) 2024 Pyarelal Knowles, MIT License

cmake_minimum_required(VERSION 3.20)

project(decodeless_header)

set(CMAKE_CXX_STANDARD 20)

option(DECODELESS_SEARCH_DEPENDENCIES "Enable searching for dependencies in adjacent directories" ON)
option(DECODELESS_FETCH_DEPENDENCIES "Enable fetching dependencies with cmake FetchContent" OFF)

if(NOT TARGET decodeless::offset_ptr)
  if(DECODELESS_SEARCH_DEPENDENCIES)
    find_package(decodeless_offset_ptr REQUIRED CONFIG PATHS "${CMAKE_CURRENT_LIST_DIR}/../offset_ptr")
  elseif(DECODELESS_FETCH_DEPENDENCIES)
    include(FetchContent)
    FetchContent_Declare(
      decodeless_offset_ptr
      GIT_REPOSITORY https://github.com/decodeless/offset_ptr.git
      GIT_TAG 1f87a9d7a8be23b90c06124014f286df91562006)
    FetchContent_MakeAvailable(decodeless_offset_ptr)
  else()
    message(
      FATAL_ERROR
        "Dependency decodeless::offset_ptr not found! You can enable searching with DECODELESS_SEARCH_DEPENDENCIES or downloading with DECODELESS_FETCH_DEPENDENCIES"
    )
  endif()
endif()

add_library(decodeless_header INTERFACE)
target_include_directories(decodeless_header INTERFACE include)
target_link_libraries(
  decodeless_header
  INTERFACE decodeless::offset_ptr)

add_library(decodeless::header ALIAS decodeless_header)

if(BUILD_TESTING)
  option(BUILD_DECODELESS_TESTING "Enable decodeless testing" ON)
  if(BUILD_DECODELESS_TESTING)
    enable_testing()
    add_subdirectory(test)
  endif()
endif()
