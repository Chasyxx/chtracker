cmake_minimum_required(VERSION 3.10)
project(ChTracker)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)

option(DISABLE_WERROR "Disable treating warnings as errors" OFF)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(VISUAL_DIR "${SRC_DIR}/visual")

set(CHTRACKER_SOURCES
    ${SRC_DIR}/channel.cxx
    ${SRC_DIR}/log.cxx
    ${SRC_DIR}/order.cxx
    ${SRC_DIR}/timer.cxx
    ${SRC_DIR}/chtracker.cxx
    ${VISUAL_DIR}/visual.c
)

if(NOT DEFINED CMAKE_C_FLAGS)
    set(CMAKE_C_FLAGS "-O2 -Wall -Wextra")
    if(NOT DISABLE_WERROR)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    endif()
endif()

find_package(SDL2 REQUIRED)
find_package(FMT REQUIRED)

include_directories(${SRC_DIR}/headers ${SDL2_INCLUDE_DIRS} ${FMT_INCLUDE_DIRS})

set(LIBRARIES SDL2::SDL2 fmt::fmt)
if(_sdl2_libraries MATCHES ".*SDL2main.*")
    list(PREPEND LIBRARIES SDL2::SDL2main)
endif()
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,windows")
    list(APPEND CHTRACKER_SOURCES ${SRC_DIR}/windowsMain.cxx)
    list(PREPEND LIBRARIES "-static")
endif()

add_executable(chtracker ${CHTRACKER_SOURCES})

add_custom_command(
    OUTPUT ${VISUAL_DIR}/font.i
    COMMAND perl ${VISUAL_DIR}/font.pl > ${VISUAL_DIR}/font.i
    DEPENDS ${VISUAL_DIR}/font.pl
    COMMENT "Generating font.i"
)

add_custom_target(generate_font_i ALL DEPENDS ${VISUAL_DIR}/font.i COMMENT "Font generation")

add_dependencies(chtracker generate_font_i)
target_link_libraries(chtracker ${LIBRARIES})
