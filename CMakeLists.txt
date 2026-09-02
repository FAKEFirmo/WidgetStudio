cmake_minimum_required(VERSION 3.24)
project(WidgetStudio VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "WidgetStudio requires the MSVC toolchain.")
endif()

include(CTest)

set(WIDGETSTUDIO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${WIDGETSTUDIO_GENERATED_DIR}")
if(PROJECT_VERSION_PATCH EQUAL 0)
    set(WIDGETSTUDIO_DISPLAY_VERSION "V${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
else()
    set(WIDGETSTUDIO_DISPLAY_VERSION "V${PROJECT_VERSION}")
endif()
file(TO_CMAKE_PATH "${WIDGETSTUDIO_GENERATED_DIR}/WidgetStudio.manifest"
    WIDGETSTUDIO_MANIFEST_RESOURCE_PATH)
configure_file(src/app/Version.h.in
    "${WIDGETSTUDIO_GENERATED_DIR}/Version.h" @ONLY)
configure_file(src/app/WidgetStudio.manifest.in
    "${WIDGETSTUDIO_GENERATED_DIR}/WidgetStudio.manifest" @ONLY)
configure_file(src/app/WidgetStudio.rc.in
    "${WIDGETSTUDIO_GENERATED_DIR}/WidgetStudio.rc" @ONLY)
file(WRITE "${WIDGETSTUDIO_GENERATED_DIR}/VERSION.txt" "${WIDGETSTUDIO_DISPLAY_VERSION}\n")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(WidgetStudio WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/StartupDiagnostics.cpp
    src/app/TrayController.cpp
    src/app/WidgetLibraryWindow.cpp
    src/app/WidgetStudioWindow.cpp
    "${WIDGETSTUDIO_GENERATED_DIR}/WidgetStudio.rc"
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
    src/rendering/RenderingResources.cpp
    src/rendering/WallpaperCache.cpp
    src/rendering/WallpaperPlacement.cpp
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

target_include_directories(WidgetStudio PRIVATE src "${WIDGETSTUDIO_GENERATED_DIR}")

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
    comctl32
    windowsapp
)

install(TARGETS WidgetStudio RUNTIME DESTINATION .)
install(FILES packaging/README.txt packaging/portable.mode LICENSE DESTINATION .)
install(FILES "${WIDGETSTUDIO_GENERATED_DIR}/VERSION.txt" DESTINATION .)
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
        src/rendering/WallpaperPlacement.cpp
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
