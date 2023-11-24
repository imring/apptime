include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

function(new_test name files)
    add_executable(${name} ${files} ${ARGN})
    target_compile_features(${name} PUBLIC cxx_std_20)
    target_link_libraries(${name} PUBLIC Catch2::Catch2)
    catch_discover_tests(${name})
endfunction()

# database
add_library(database ../src/database.cpp)
target_include_directories(database PUBLIC ../src/)
target_compile_features(database PUBLIC cxx_std_20)
target_link_libraries(database PUBLIC SQLiteCpp SQLite::SQLite3)

new_test(database-test database.cpp)
target_link_libraries(database-test PUBLIC database)