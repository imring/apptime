include(FetchContent)

set(SQLITECPP_RUN_CPPLINT OFF)
FetchContent_Declare(
    SQLiteCpp

    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG        3.3.1
)
FetchContent_MakeAvailable(SQLiteCpp)

# process
add_library(apptime-process)
target_compile_features(apptime-process PUBLIC cxx_std_20)
target_include_directories(apptime-process PUBLIC .)

# database
add_library(apptime-database
    database/database.cpp
    database/database_sqlite.cpp
)
target_compile_features(apptime-database PUBLIC cxx_std_20)
target_include_directories(apptime-database PUBLIC .)
target_link_libraries(apptime-database PUBLIC
    SQLiteCpp
    SQLite::SQLite3
)

# monitoring
add_library(apptime-monitoring monitoring.cpp)
target_compile_features(apptime-monitoring PUBLIC cxx_std_20)
target_include_directories(apptime-monitoring PUBLIC .)
target_link_libraries(apptime-monitoring PUBLIC apptime-process apptime-database)

# apptime
add_executable(apptime
    database/database_sqlite.cpp

    gui/widgets/records.cpp
    gui/ignore.cpp
    gui/settings.cpp
    gui/tray.cpp
    gui/window.cpp

    main.cpp
)

if(WIN32)
    target_sources(apptime-process PRIVATE process/process_system_win32.cpp)

    target_sources(apptime PRIVATE
        platforms/encoding_win32.cpp
        platforms/icon_win32.cpp
    )
elseif(UNIX)
    target_sources(apptime-process PRIVATE process/process_system_unix.cpp)
    target_link_libraries(apptime-process PUBLIC PkgConfig::PROCPS)

    target_sources(apptime PRIVATE platforms/icon_unix.cpp)
endif()

target_include_directories(apptime PRIVATE .)
target_link_libraries(apptime PRIVATE
    apptime-monitoring

    Qt${QT_VERSION}::Widgets
)
target_compile_definitions(apptime PRIVATE
    $<$<CONFIG:DEBUG>:DEBUG_MODE>
    $<$<CONFIG:RELEASE>:QT_NO_DEBUG_OUTPUT>
)
target_compile_features(apptime PRIVATE cxx_std_20)

if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET apptime PROPERTY WIN32_EXECUTABLE true)
endif()

qt_add_resources(apptime "assets"
    PREFIX "/"
    BASE "../assets"
    FILES ../assets/icon.png
)
qt_add_resources(apptime "styles"
    PREFIX "/"
    BASE "gui"
    FILES gui/styles/records.qss
)