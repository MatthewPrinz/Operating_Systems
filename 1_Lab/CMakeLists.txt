cmake_minimum_required(VERSION 3.17)
project(1_Lab C)

set(CMAKE_C_STANDARD 99)

add_executable(1_Lab yash.c)

target_link_libraries(1_Lab /lib/x86_64-linux-gnu/libreadline.so.8.0)