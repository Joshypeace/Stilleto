cmake_minimum_required(VERSION 3.25)

project(Stilleto VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static MSVC runtime everywhere (JUCE, ggml, whisper and Stilleto) so the executable does not
# depend on a VC++ redistributable and no runtime mismatch can occur between the libraries.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT WIN32)
    message(FATAL_ERROR "Stilleto is a Windows application (global hotkeys, SendInput, tray).")
endif()

option(STILLETO_BUILD_APP   "Build the Stilleto JUCE application" ON)
option(STILLETO_BUILD_TESTS "Build the Stilleto test executables" ON)
option(STILLETO_GPU_VULKAN  "Build whisper.cpp with the Vulkan backend (requires the Vulkan SDK)" OFF)
option(STILLETO_NATIVE_CPU  "Optimise ggml for this machine's CPU only (not suitable for distribution)" OFF)

#-----------------------------------------------------------------------------------------------
# JUCE
#-----------------------------------------------------------------------------------------------
# Prefer a vendored copy in external/JUCE; otherwise use the shared checkout next to the projects
# folder (D:/Audio Development/JUCE in the reference environment).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    set(_stilleto_default_juce "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")
else()
    set(_stilleto_default_juce "${CMAKE_CURRENT_SOURCE_DIR}/../../JUCE")
endif()
set(STILLETO_JUCE_DIR "${_stilleto_default_juce}" CACHE PATH "Path to the JUCE framework checkout")

if(NOT EXISTS "${STILLETO_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at '${STILLETO_JUCE_DIR}'. Set -DSTILLETO_JUCE_DIR=<path>.")
endif()

add_subdirectory("${STILLETO_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

#-----------------------------------------------------------------------------------------------
# whisper.cpp (git submodule, pinned to a release tag)
#-----------------------------------------------------------------------------------------------
set(STILLETO_WHISPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/whisper.cpp" CACHE PATH "Path to whisper.cpp")
if(NOT EXISTS "${STILLETO_WHISPER_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "whisper.cpp not found. Run: git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS        OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER     OFF CACHE BOOL "" FORCE)
set(WHISPER_ALL_WARNINGS     OFF CACHE BOOL "" FORCE)
set(WHISPER_CURL             OFF CACHE BOOL "" FORCE)
set(WHISPER_SDL2             OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE ${STILLETO_NATIVE_CPU} CACHE BOOL "" FORCE)
set(GGML_VULKAN ${STILLETO_GPU_VULKAN} CACHE BOOL "" FORCE)
# OpenMP would pull in vcomp140.dll (a redistributable); ggml's own thread pool is used instead.
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
set(GGML_CCACHE OFF CACHE BOOL "" FORCE)

add_subdirectory("${STILLETO_WHISPER_DIR}" "${CMAKE_BINARY_DIR}/whisper" EXCLUDE_FROM_ALL)

#-----------------------------------------------------------------------------------------------
# StilletoCore: pure C++ logic with no JUCE or Win32 dependency (unit tested)
#-----------------------------------------------------------------------------------------------
add_library(StilletoCore STATIC
    src/core/DictationState.h
    src/core/DictationState.cpp
    src/core/HotkeySpec.h
    src/core/HotkeySpec.cpp
    src/core/TextProcessing.h
    src/core/TextProcessing.cpp
    src/core/Sha1.h
    src/core/Sha1.cpp
    src/core/SpeechDetection.h
    src/core/SpeechDetection.cpp
    src/audio/AudioResampler.h
    src/audio/AudioResampler.cpp
    src/transcription/WhisperEngine.h
    src/transcription/WhisperEngine.cpp
    src/transcription/ModelCatalog.h
    src/transcription/ModelCatalog.cpp)

target_include_directories(StilletoCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(StilletoCore PUBLIC whisper)
target_compile_definitions(StilletoCore PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(StilletoCore PRIVATE /W4 /permissive- /utf-8)

#-----------------------------------------------------------------------------------------------
# StilletoPlatform: Win32 integration with no JUCE dependency (hotkeys, input injection,
# clipboard, foreground tracking, native window styles). Exercised by the integration tests.
#-----------------------------------------------------------------------------------------------
add_library(StilletoPlatform STATIC
    src/windows/GlobalHotkey.h
    src/windows/GlobalHotkey.cpp
    src/windows/TextInserter.h
    src/windows/TextInserter.cpp
    src/windows/ForegroundTracker.h
    src/windows/ForegroundTracker.cpp
    src/windows/NativeWindow.h
    src/windows/NativeWindow.cpp)

target_link_libraries(StilletoPlatform PUBLIC StilletoCore user32 dwmapi advapi32 shell32)
target_compile_definitions(StilletoPlatform PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(StilletoPlatform PRIVATE /W4 /permissive- /utf-8)

if(STILLETO_BUILD_APP)

#-----------------------------------------------------------------------------------------------
# Stilleto application
#-----------------------------------------------------------------------------------------------
# assets/StilletoIcon.png is rendered from the same vector brand mark the UI draws
# (tools/IconExporter). It is optional so a fresh checkout configures before it exists.
set(_stilleto_icon_args)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/StilletoIcon.png")
    set(_stilleto_icon_args ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/StilletoIcon.png")
endif()

juce_add_gui_app(Stilleto
    PRODUCT_NAME "Stilleto"
    COMPANY_NAME "Stilleto"
    BUNDLE_ID "com.stilleto.app"
    VERSION ${PROJECT_VERSION}
    ${_stilleto_icon_args}
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(Stilleto PRIVATE
    src/Main.cpp
    src/AppController.h
    src/AppController.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/theme/DesignTokens.h
    src/theme/Animation.h
    src/theme/Icons.h
    src/theme/Icons.cpp
    src/theme/StilletoLookAndFeel.h
    src/theme/StilletoLookAndFeel.cpp
    src/components/Card.h
    src/components/Buttons.h
    src/components/Buttons.cpp
    src/components/Sidebar.h
    src/components/Sidebar.cpp
    src/components/TopBar.h
    src/components/TopBar.cpp
    src/components/WaveformView.h
    src/components/WaveformView.cpp
    src/components/StatusIndicator.h
    src/components/StatusIndicator.cpp
    src/components/StatCard.h
    src/components/StatCard.cpp
    src/components/MicOrb.h
    src/components/MicOrb.cpp
    src/components/RecordingWidget.h
    src/components/RecordingWidget.cpp
    src/components/HotkeyEditor.h
    src/components/HotkeyEditor.cpp
    src/pages/Page.h
    src/pages/OverviewPage.h
    src/pages/OverviewPage.cpp
    src/pages/DictationPage.h
    src/pages/DictationPage.cpp
    src/pages/HistoryPage.h
    src/pages/HistoryPage.cpp
    src/pages/ModelsPage.h
    src/pages/ModelsPage.cpp
    src/pages/SettingsPage.h
    src/pages/SettingsPage.cpp
    src/pages/AboutPage.h
    src/pages/AboutPage.cpp
    src/audio/AudioRecorder.h
    src/audio/AudioRecorder.cpp
    src/transcription/WhisperTranscriber.h
    src/transcription/WhisperTranscriber.cpp
    src/transcription/ModelManager.h
    src/transcription/ModelManager.cpp
    src/windows/TrayIcon.h
    src/windows/TrayIcon.cpp
    src/history/HistoryManager.h
    src/history/HistoryManager.cpp
    src/settings/SettingsManager.h
    src/settings/SettingsManager.cpp)

target_compile_definitions(Stilleto PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_WINRT_MIDI=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Stilleto,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Stilleto,JUCE_VERSION>")

target_compile_options(Stilleto PRIVATE /utf-8)

target_link_libraries(Stilleto
    PRIVATE
        StilletoCore
        StilletoPlatform
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

endif() # STILLETO_BUILD_APP

#-----------------------------------------------------------------------------------------------
# Tests
#-----------------------------------------------------------------------------------------------
if(STILLETO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
