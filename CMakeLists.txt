cmake_minimum_required(VERSION 3.24)
project(WidgetStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(WidgetStudio WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/TrayController.cpp
    src/app/WidgetLibraryWindow.cpp
    src/desktop/DesktopHost.cpp
    src/layout/GridLayout.cpp
    src/rendering/Renderer.cpp
    src/scene/WidgetScene.cpp
    src/widgets/DebugWidget.cpp
    src/widgets/WidgetRegistry.cpp
)

target_include_directories(WidgetStudio PRIVATE src)

target_compile_definitions(WidgetStudio PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(WidgetStudio PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_link_libraries(WidgetStudio PRIVATE
    d2d1
    dwrite
    windowscodecs
    shell32
    ole32
    user32
    gdi32
)
