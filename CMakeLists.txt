cmake_minimum_required(VERSION 3.24)
project(WidgetStudio VERSION 1.0.0 LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "WidgetStudio requires the MSVC toolchain.")
endif()

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(WidgetStudio WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/TrayController.cpp
    src/app/WidgetLibraryWindow.cpp
    src/app/WidgetStudioWindow.cpp
    src/app/WidgetStudio.rc
    src/desktop/DesktopBackendController.cpp
    src/desktop/DesktopHost.cpp
    src/desktop/WidgetWindow.cpp
    src/desktop/WidgetWindowPlacement.cpp
    src/layout/GridLayout.cpp
    src/layout/AuthoredContentLayout.cpp
    src/layout/Alignment.cpp
    src/layout/OuterLayout.cpp
    src/persistence/SceneJsonCodec.cpp
    src/persistence/SceneStore.cpp
    src/persistence/AssetLibrary.cpp
    src/rendering/Renderer.cpp
    src/scene/WidgetScene.cpp
    src/widgets/BuiltInWidgets.cpp
    src/widgets/CalendarWidget.cpp
    src/widgets/ClockWidget.cpp
    $<$<CONFIG:Debug>:src/widgets/DebugWidget.cpp>
    src/widgets/MusicWidget.cpp
    src/widgets/PhotoWidget.cpp
    src/widgets/WidgetRegistry.cpp
    src/widgets/calendar/CalendarModel.cpp
    src/widgets/photo/PhotoLayout.cpp
    src/windows/MediaSessionService.cpp
    src/windows/MonitorTopology.cpp
    src/windows/StartupShortcutService.cpp
)

target_include_directories(WidgetStudio PRIVATE src)

target_compile_definitions(WidgetStudio PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

if(MSVC)
    target_compile_options(WidgetStudio PRIVATE /W4 /permissive- /EHsc /utf-8)
    # WidgetStudio.rc embeds the authoritative Per-Monitor V2 application manifest.
    target_link_options(WidgetStudio PRIVATE /MANIFEST:NO)
endif()

target_link_libraries(WidgetStudio PRIVATE
    d2d1
    dwrite
    windowscodecs
    shell32
    shcore
    ole32
    user32
    gdi32
    windowsapp
)

install(TARGETS WidgetStudio RUNTIME DESTINATION .)
install(FILES packaging/README.txt packaging/portable.mode DESTINATION .)
install(CODE [[
    file(MAKE_DIRECTORY
        "${CMAKE_INSTALL_PREFIX}/assets"
        "${CMAKE_INSTALL_PREFIX}/data/config"
        "${CMAKE_INSTALL_PREFIX}/data/images"
        "${CMAKE_INSTALL_PREFIX}/data/cache")
]])

if(BUILD_TESTING)
    add_executable(WidgetStudioLogicTests
        tests/LogicTests.cpp
        src/desktop/WidgetWindowPlacement.cpp
        src/layout/GridLayout.cpp
        src/layout/AuthoredContentLayout.cpp
        src/layout/Alignment.cpp
        src/layout/OuterLayout.cpp
        src/persistence/AssetLibrary.cpp
        src/persistence/SceneJsonCodec.cpp
        src/persistence/SceneStore.cpp
        src/scene/WidgetScene.cpp
        src/widgets/ClockWidget.cpp
        src/widgets/WidgetRegistry.cpp
        src/widgets/calendar/CalendarModel.cpp
        src/widgets/photo/PhotoLayout.cpp
        src/windows/MonitorTopology.cpp
    )
    target_include_directories(WidgetStudioLogicTests PRIVATE src)
    target_compile_definitions(WidgetStudioLogicTests PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    if(MSVC)
        target_compile_options(WidgetStudioLogicTests PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()
    target_link_libraries(WidgetStudioLogicTests PRIVATE d2d1 dwrite shell32 shcore ole32)
    add_test(NAME WidgetStudioLogicTests COMMAND WidgetStudioLogicTests)
endif()
