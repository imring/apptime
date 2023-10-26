cmake_minimum_required(VERSION 2.20)
project(apptime)

find_package(SQLite3 REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)

add_subdirectory(src)