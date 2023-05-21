cmake_minimum_required(VERSION 3.24)
project(try C)

set(CMAKE_C_STANDARD 11)

add_executable(try server.c cashier_1.c customer_generator.c cashier_2.c)
