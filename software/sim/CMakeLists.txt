cmake_minimum_required(VERSION 3.8)

project(microgame_sim CXX ASM)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/;${CMAKE_MODULE_PATH}")

find_package(SimAvr)
find_package(Boost REQUIRED COMPONENTS program_options regex)

include_directories(${SIMAVR_INCLUDE_DIR})
include_directories(${SIMAVR_INCLUDE_DIR}/avr)
include_directories(${SIMAVR_INCLUDE_DIR}/parts)
include_directories(${Boost_INCLUDE_DIRS})

add_compile_options(-Wall -Wextra -fpermissive -O3)

link_libraries(simavrparts)
link_libraries(ncurses)
link_libraries(simavr)
link_libraries(${Boost_LIBRARIES})

add_executable(sim
    main.cpp
)

# set_target_properties(sim PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
