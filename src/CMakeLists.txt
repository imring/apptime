add_executable(apptime
    database.cpp
    main.cpp
    monitoring.cpp
)
if (WIN32)
    target_sources(apptime PRIVATE process_win32.cpp)
endif()

target_compile_features(apptime PRIVATE cxx_std_20)
target_link_libraries(apptime PRIVATE SQLiteCpp SQLite::SQLite3)