add_executable(apptime main.cpp)
if (WIN32)
    target_sources(apptime PRIVATE process_win32.cpp)
endif()

target_compile_features(apptime PRIVATE cxx_std_20)