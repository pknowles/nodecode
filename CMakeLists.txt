# Copyright (c) 2024 Pyarelal Knowles, MIT License

cmake_minimum_required(VERSION 3.20)

project(nodecode)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(offset_ptr)
if(BUILD_TESTING)
  option(BUILD_NODECODE_HEADER_TESTING "Enable nodecode_header testing" ON)
  if(BUILD_NODECODE_HEADER_TESTING)
    enable_testing()
    add_subdirectory(test)
  endif()
endif()

add_library(nodecode_header INTERFACE)
target_include_directories(nodecode_header INTERFACE include)
target_link_libraries(nodecode_header INTERFACE offset_ptr)
