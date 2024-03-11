cmake_minimum_required(VERSION 3.20)
project(apptime VERSION 0.10)

option(APPTIME_TEST "Enable testing." ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(QT_VERSION 6)

find_package(Qt${QT_VERSION} REQUIRED COMPONENTS Widgets)

if(UNIX)
    include(FindPkgConfig)
    pkg_check_modules(PROCPS REQUIRED IMPORTED_TARGET libprocps)
endif()

if (MSVC)
    # warning level 4
    add_compile_options(/W4)
else()
    # additional warnings
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src)

if (APPTIME_TEST)
    enable_testing()
    add_subdirectory(tests)
endif()