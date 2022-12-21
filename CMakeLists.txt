cmake_minimum_required(VERSION 3.24)
project(P2P_FileShare C)

set(CMAKE_C_STANDARD 11)

add_executable(P2P_FileShare Server/FSserver.c)
