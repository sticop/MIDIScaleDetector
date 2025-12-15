cmake_minimum_required(VERSION 3.20)

project(MIDIScaleDetector VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS specific settings
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS")

# JUCE setup
# Download JUCE if not present
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(STATUS "JUCE not found. Please download JUCE 7.0+ and place in ${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    message(STATUS "Or clone from: git clone https://github.com/juce-framework/JUCE.git")
endif()

# Add JUCE
add_subdirectory(JUCE)

# Global compile options
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:DEBUG>:-g>
    $<$<CONFIG:RELEASE>:-O3>
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
)

# Core library (shared between standalone and plugin)
add_subdirectory(Source/Core)

# Standalone application
option(BUILD_STANDALONE "Build standalone macOS application" ON)
if(BUILD_STANDALONE)
    add_subdirectory(Source/Standalone)
endif()

# Audio plugins
option(BUILD_VST3 "Build VST3 plugin" ON)
option(BUILD_AU "Build AudioUnit plugin" ON)
if(BUILD_VST3 OR BUILD_AU)
    add_subdirectory(Source/Plugin)
endif()

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# Installation
install(TARGETS MIDIScaleDetectorCore
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Summary
message(STATUS "")
message(STATUS "MIDIScaleDetector Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Standalone: ${BUILD_STANDALONE}")
message(STATUS "  Build VST3: ${BUILD_VST3}")
message(STATUS "  Build AU: ${BUILD_AU}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  macOS Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
message(STATUS "")
