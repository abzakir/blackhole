cmake_minimum_required(VERSION 3.24)
project(Blackhole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The simulation is maths-heavy, so build optimised unless asked otherwise.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SFML 3 COMPONENTS Graphics REQUIRED)

# Smoke test: checks that SFML, OpenGL and shaders work on this machine.
add_executable(sfml_test sfml_test.cpp)
target_link_libraries(sfml_test PRIVATE SFML::Graphics)

