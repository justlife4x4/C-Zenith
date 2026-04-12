cmake_minimum_required(VERSION 3.10)
project(Zenith C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Includes
include_directories(include)

# Source Files
set(ZENITH_SOURCES
    src/zenith_core.c
    src/zenith_mem.c
    src/zenith_shadow.c
)

if(UNIX)
    list(APPEND ZENITH_SOURCES src/zenith_posix.c)
endif()

# Library Target
add_library(zenith STATIC ${ZENITH_SOURCES})

# Installation (Example)
install(TARGETS zenith DESTINATION lib)
install(FILES include/zenith.h DESTINATION include)
