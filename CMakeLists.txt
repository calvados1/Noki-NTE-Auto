cmake_minimum_required(VERSION 3.20)
project(NokiLauncher)

set(CMAKE_CXX_STANDARD 17)

add_executable(NokiLauncher WIN32
    main.cpp
    resources.rc
)

target_link_libraries(NokiLauncher PRIVATE
    comctl32
    shell32
    dwmapi
    uxtheme
)

# MSVC: UTF-8 source, no console window
if (MSVC)
    target_compile_options(NokiLauncher PRIVATE /utf-8)
    set_target_properties(NokiLauncher PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()
