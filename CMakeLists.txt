cmake_minimum_required(VERSION 3.8)
project(NearJS)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES_LIB source/near.cpp include/near.hpp)
add_library(NearJS SHARED ${SOURCE_FILES_LIB})
include_directories(../node-v6.11.4/include/node)
target_link_libraries(NearJS node -lpthread)