cmake_minimum_required(VERSION 3.24)
project(point6_7 C)

set(CMAKE_C_STANDARD 11)

add_executable(point6_7 cashier_1.c server.c observer.c cashier_2.c customer_generator.c)
