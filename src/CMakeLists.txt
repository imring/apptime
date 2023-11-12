add_executable(apptime
    gui/table.cpp
    gui/tray.cpp
    gui/window.cpp

    database.cpp
    main.cpp
    monitoring.cpp
)

if(WIN32)
    target_sources(apptime PRIVATE process_win32.cpp)
elseif(UNIX)
    target_link_libraries(apptime PRIVATE PkgConfig::PROCPS)
    target_sources(apptime PRIVATE process_unix.cpp)
endif()

target_compile_features(apptime PRIVATE cxx_std_20)
target_link_libraries(apptime PRIVATE
    SQLiteCpp
    SQLite::SQLite3

    Qt${QT_VERSION}::Widgets
)
target_compile_definitions(apptime PRIVATE
    $<$<CONFIG:DEBUG>:DEBUG_MODE>
    $<$<CONFIG:RELEASE>:QT_NO_DEBUG_OUTPUT>
)