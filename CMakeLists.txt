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

# Physics check: the original geodesic.comp equations versus the textbook ones.
add_executable(shadow_check checks/shadow_check.cpp)

# Tutorial steps: each one is its own program, built from steps/<name>/main.cpp.
function(add_step name)
    add_executable(${name} steps/${name}/main.cpp)
    target_include_directories(${name} PRIVATE steps/common)
    target_link_libraries(${name} PRIVATE SFML::Graphics)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
endfunction()

add_step(01_straight_rays)
add_step(02_event_horizon)
add_step(03_bending_light)
