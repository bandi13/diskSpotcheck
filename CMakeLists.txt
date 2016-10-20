cmake_minimum_required(VERSION 3.6)
project(diskSpotcheck)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O4 -Wall")

add_executable(diskSpotCheck diskSpotcheck.cpp)
add_executable(filesystemTest filesystemTest.cpp)
add_executable(fst fst.cpp)
target_link_libraries(fst pthread)
