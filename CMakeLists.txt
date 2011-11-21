cmake_minimum_required(VERSION 2.6)
project(ago)
add_library(ago ago.cpp)
add_executable(ago_test ago_test.cpp)
target_link_libraries(ago_test ago)

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "-std=c++0x -pthread")
endif()


