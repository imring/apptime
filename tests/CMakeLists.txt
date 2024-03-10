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

# database (integration test)
new_test(database-test database_test.cpp)
target_link_libraries(database-test PUBLIC apptime-database)

# monitoring (unit test)
new_test(monitoring-test monitoring_test.cpp)
target_link_libraries(monitoring-test PUBLIC apptime-monitoring)