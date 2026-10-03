cmake_minimum_required(VERSION 3.28)

# Homebrew's Qt 6 is built for macOS 14. Release builds pass their own value.
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version")

project(NXSampler VERSION 0.0.1 LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJC OBJCXX) # the SDK's macOS module loader is Objective-C++
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    # Source files are UTF-8 (…, –), and windows.h must not define min/max macros.
    add_compile_options(/utf-8)
    add_compile_definitions(NOMINMAX)
endif()

# Live audio uses Core Audio on macOS and Qt Multimedia elsewhere.
if(APPLE)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Test)
else()
    find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Test Multimedia)
endif()
qt_standard_project_setup()

# --- Steinberg VST3 SDK (MIT) -------------------------------------------------
# Only the hosting libraries are needed; examples, VSTGUI and utilities are skipped.
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_UTILITIES OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

# The SDK reads the Xcode version from `xcodebuild`, which fails when the active developer
# directory is the Command Line Tools. It only needs a version for its "Xcode 9+" check.
if(APPLE AND NOT DEFINED ENV{XCODE_VERSION})
    execute_process(COMMAND xcodebuild -version RESULT_VARIABLE _xcodebuild_result OUTPUT_QUIET ERROR_QUIET)
    if(NOT _xcodebuild_result EQUAL 0)
        set(ENV{XCODE_VERSION} "${CMAKE_CXX_COMPILER_VERSION}")
        set(XCODE_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
    endif()
endif()

include(FetchContent)
FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.8.1_build_84
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES base pluginterfaces public.sdk cmake
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(vst3sdk)

# Host-side pieces that sdk_hosting leaves to the application: the plug-in provider and
# the platform's module (bundle/DLL) loader.
set(_hosting ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting)
if(APPLE)
    set(_module_loader ${_hosting}/module_mac.mm)
    set_source_files_properties(${_module_loader} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
elseif(WIN32)
    set(_module_loader ${_hosting}/module_win32.cpp)
else()
    set(_module_loader ${_hosting}/module_linux.cpp)
endif()
add_library(nx_vst3_hosting STATIC ${_hosting}/plugprovider.cpp ${_module_loader})
target_link_libraries(nx_vst3_hosting PUBLIC sdk_hosting)
if(APPLE)
    target_link_libraries(nx_vst3_hosting PUBLIC "-framework CoreFoundation" "-framework Foundation")
elseif(UNIX)
    target_link_libraries(nx_vst3_hosting PUBLIC ${CMAKE_DL_LIBS})
endif()

# --- Core library (no UI) -----------------------------------------------------
add_library(nx_core STATIC
    src/AudioOps.cpp
    src/AudioOps.h
    src/LoopFinder.cpp
    src/LoopFinder.h
    src/WavWriter.cpp
    src/WavWriter.h
)
target_include_directories(nx_core PUBLIC src)
target_link_libraries(nx_core PUBLIC Qt6::Core)

# --- Application --------------------------------------------------------------
qt_add_executable(NXSampler MACOSX_BUNDLE WIN32
    src/main.cpp
    src/AudioMonitor.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/KeyboardWidget.cpp
    src/KeyboardWidget.h
    src/Vst3Scanner.cpp
    src/Vst3Scanner.h
    src/Vst3Host.cpp
    src/Vst3Host.h
    src/PluginEditorWindow.cpp
    src/PluginEditorWindow.h
    src/ExtractWorker.cpp
    src/ExtractWorker.h
)
set_target_properties(NXSampler PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "NXSampler"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.nxsampler.app"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)
target_link_libraries(NXSampler PRIVATE nx_core nx_vst3_hosting Qt6::Widgets)
target_compile_definitions(NXSampler PRIVATE NXSAMPLER_VERSION="${PROJECT_VERSION}")
if(APPLE)
    target_sources(NXSampler PRIVATE src/AudioMonitorMac.cpp)
    target_link_libraries(NXSampler PRIVATE "-framework AudioToolbox" "-framework CoreAudio")
else()
    target_sources(NXSampler PRIVATE src/AudioMonitorQt.cpp)
    target_link_libraries(NXSampler PRIVATE Qt6::Multimedia)
endif()

install(TARGETS NXSampler BUNDLE DESTINATION . RUNTIME DESTINATION .)

# --- Tests --------------------------------------------------------------------
enable_testing()

qt_add_executable(test_audioops tests/test_audioops.cpp)
target_link_libraries(test_audioops PRIVATE nx_core Qt6::Test)
add_test(NAME test_audioops COMMAND test_audioops)

qt_add_executable(test_wavwriter tests/test_wavwriter.cpp)
target_link_libraries(test_wavwriter PRIVATE nx_core Qt6::Test)
add_test(NAME test_wavwriter COMMAND test_wavwriter)

qt_add_executable(test_loopfinder tests/test_loopfinder.cpp)
target_link_libraries(test_loopfinder PRIVATE nx_core Qt6::Test)
add_test(NAME test_loopfinder COMMAND test_loopfinder)
