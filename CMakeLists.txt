cmake_minimum_required(VERSION 3.24)
project(Blackhole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release build by default.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SFML 3 COMPONENTS Graphics REQUIRED)
find_package(OpenGL REQUIRED)

# Checks that SFML, OpenGL and shaders work.
add_executable(sfml_test sfml_test.cpp)
target_link_libraries(sfml_test PRIVATE SFML::Graphics)

# Compares the original shader's equations with the textbook ones.
add_executable(shadow_check checks/shadow_check.cpp)

# One program per step: steps/<name>/main.cpp.
function(add_step name)
    add_executable(${name} steps/${name}/main.cpp steps/common/use_fast_gpu.cpp)
    target_include_directories(${name} PRIVATE steps/common)
    target_link_libraries(${name} PRIVATE SFML::Graphics OpenGL::GL)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    # Shaders load from the source folder, so editing one only needs F5.
    target_compile_definitions(${name} PRIVATE STEP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/steps/${name}")
endfunction()

add_step(01_straight_rays)
add_step(02_event_horizon)
add_step(03_bending_light)
add_step(04_rk4)
add_step(05_shadow_3d)
add_step(06_accretion_disk)
