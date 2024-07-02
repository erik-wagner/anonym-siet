cmake_minimum_required(VERSION 3.18)
project(untitled1 C)

set(CMAKE_C_STANDARD 99)

add_executable(serv server.c)
add_executable(client client.c)
add_executable(node1 client.c)
add_executable(node2 client.c)
add_executable(node3 client.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")