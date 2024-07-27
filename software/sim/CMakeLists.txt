cmake_minimum_required(VERSION 3.8)

project(microgame_sim CXX ASM)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/;${CMAKE_MODULE_PATH}")

find_package(SimAvr)

include_directories(${SIMAVR_INCLUDE_DIR})
include_directories(${SIMAVR_INCLUDE_DIR}/avr)
include_directories(${SIMAVR_INCLUDE_DIR}/parts)

add_compile_options(-Wall -Wextra -fpermissive -O3)

link_libraries(simavrparts)
link_libraries(ncurses)
link_libraries(simavr)

add_executable(${PROJECT_NAME} 
    main.cpp
)

