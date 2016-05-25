cmake_minimum_required(VERSION 3.5)
project(terrariashot)

set(CMAKE_C_FLAGS "-O3 -std=c11 -pedantic -Wall -Wextra -Werror -pipe")
#set(CMAKE_VERBOSE_MAKEFILE ON)

set(SOURCE_FILES terrariashot.c world.c world.h error.c error.h tile.c tile.h)

add_executable(terrariashot ${SOURCE_FILES})
