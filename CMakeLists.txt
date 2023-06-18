cmake_minimum_required(VERSION 3.23)
project(wyn)

set(CMAKE_C_STANDARD 23)

add_executable(example)
target_sources(example PRIVATE "example/main.c")
