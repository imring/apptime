add_executable(apptime
    gui/ignore.cpp
    gui/records.cpp
    gui/tray.cpp
    gui/window.cpp

    database.cpp
    main.cpp
    monitoring.cpp
)

if(WIN32)
    target_sources(apptime PRIVATE platforms/encoding_win32.cpp)
    target_sources(apptime PRIVATE platforms/icon_win32.cpp)
    target_sources(apptime PRIVATE platforms/process_win32.cpp)
elseif(UNIX)
    target_link_libraries(apptime PRIVATE PkgConfig::PROCPS)
    target_sources(apptime PRIVATE platforms/process_unix.cpp)
endif()

target_include_directories(apptime PRIVATE .)
target_link_libraries(apptime PRIVATE
    SQLiteCpp
    SQLite::SQLite3

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
    FILES
        ../assets/icon.png
)