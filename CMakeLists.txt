cmake_minimum_required(VERSION 3.0)
project(diskSpotcheck)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O4 -Wall -g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG") # disable assertions
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Ofast -g")
set(CMAKE_CXX_FLAGS_DEBUG "-DENABLE_DEBUG_CODE -g -pg -Og")

add_executable(diskSystemTest blockDeviceTests/diskSystemTest.cpp)
target_link_libraries(diskSystemTest pthread)
add_executable(diskSpotCheck blockDeviceTests/diskSpotcheck.cpp)
add_executable(filesystemTest filesystemTests/filesystemTest.cpp)
add_executable(fst filesystemTests/fst.cpp)
target_link_libraries(fst pthread)
