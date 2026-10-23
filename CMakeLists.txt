cmake_minimum_required(VERSION 3.25)

project(PerfPulse
    VERSION 0.1.0
    DESCRIPTION "Lightweight game performance recorder for Windows"
    LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "PerfPulse targets Windows only.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT: ship a single .exe without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(PERFPULSE_BUILD_APP "Build the tray application" ON)
option(PERFPULSE_BUILD_TESTS "Build the unit tests" ON)

configure_file(src/core/version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/core/version.hpp" @ONLY)

# ---------------------------------------------------------------------------
# perfpulse_core: platform-light logic (config, json, analysis, report).
# Kept separate from the Win32 shell so it can be unit tested.
# ---------------------------------------------------------------------------
add_library(perfpulse_core STATIC
    src/core/config.cpp
    src/core/i18n.cpp
    src/core/json.cpp
    src/core/metrics.cpp
    src/core/strings.cpp
    src/analysis/bottleneck_analyzer.cpp
    src/analysis/energy_analyzer.cpp
    src/analysis/frame_statistics.cpp
    src/analysis/session_analysis.cpp
    src/analysis/smoothness_score.cpp
    src/analysis/stutter_detector.cpp
    src/report/history_store.cpp
    src/report/html_report_writer.cpp
    src/report/report_json.cpp
    src/report/report_reanalysis.cpp
)
target_include_directories(perfpulse_core PUBLIC
    src
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
perfpulse_set_compiler_options(perfpulse_core)

# ---------------------------------------------------------------------------
# perfpulse: the tray application.
# ---------------------------------------------------------------------------
if(PERFPULSE_BUILD_APP)
add_executable(perfpulse WIN32
    src/main.cpp
    src/app/application.cpp
    src/app/autostart.cpp
    src/app/diagnostics.cpp
    src/app/icon_factory.cpp
    src/app/settings_window.cpp
    src/app/theme.cpp
    src/app/toggle_switch.cpp
    src/app/tray_icon.cpp
    src/capture/frame_capture.cpp
    src/core/log.cpp
    src/core/paths.cpp
    src/detection/game_detector.cpp
    src/detection/monitor_info.cpp
    src/detection/process_info.cpp
    src/report/demo_session.cpp
    src/report/report_generator.cpp
    src/report/resource_loader.cpp
    src/sensors/cpu_sensors.cpp
    src/sensors/generic_gpu.cpp
    src/sensors/hwinfo_source.cpp
    src/sensors/lhm_source.cpp
    src/sensors/memory_sensors.cpp
    src/sensors/nvidia_gpu.cpp
    src/sensors/pawnio_source.cpp
    src/sensors/sensor_hub.cpp
    src/sensors/system_info.cpp
    src/session/session_recorder.cpp
    resources/perfpulse.rc
)
set_target_properties(perfpulse PROPERTIES OUTPUT_NAME "PerfPulse")
# The resource compiler resolves icon/template paths through the include path.
target_include_directories(perfpulse PRIVATE resources)
set_property(SOURCE resources/perfpulse.rc APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/report/report_template.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/report/index_template.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/perfpulse.manifest"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/perfpulse.ico")
target_link_libraries(perfpulse PRIVATE
    perfpulse_core
    advapi32 comctl32 comdlg32 dwmapi dxgi gdi32 gdiplus ole32 oleaut32
    pdh shell32 shlwapi user32 uxtheme version wbemuuid)
target_link_options(perfpulse PRIVATE /MANIFEST:NO)
perfpulse_set_compiler_options(perfpulse)

# PawnIO modules (fetched by tools/fetch_pawnio_modules.ps1) live next to the exe.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules")
    add_custom_command(TARGET perfpulse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/modules" "$<TARGET_FILE_DIR:perfpulse>/modules")
endif()
endif()

if(PERFPULSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
