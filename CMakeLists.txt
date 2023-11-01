cmake_minimum_required(VERSION 2.20)
project(apptime)

set(CMAKE_AUTOMOC ON)
set(QT_VERSION 6)

find_package(SQLite3 REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(Qt${QT_VERSION} REQUIRED COMPONENTS Widgets)

if(UNIX)
    include(FindPkgConfig)
    pkg_check_modules(PROCPS REQUIRED IMPORTED_TARGET libprocps)
endif()

add_subdirectory(src)