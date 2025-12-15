cmake_minimum_required(VERSION 3.20)

project(MIDIScaleDetectorCore)

# Core library sources
set(CORE_SOURCES
    MIDIParser/MIDIParser.cpp
    ScaleDetector/ScaleDetector.cpp
    Database/Database.cpp
    FileScanner/FileScanner.cpp
)

set(CORE_HEADERS
    MIDIParser/MIDIParser.h
    ScaleDetector/ScaleDetector.h
    Database/Database.h
    FileScanner/FileScanner.h
)

# Create static library
add_library(MIDIScaleDetectorCore STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Link libraries
target_link_libraries(MIDIScaleDetectorCore
    PUBLIC
        SQLite::SQLite3
)

# Include directories
target_include_directories(MIDIScaleDetectorCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Compiler features
target_compile_features(MIDIScaleDetectorCore PUBLIC cxx_std_17)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(MIDIScaleDetectorCore PUBLIC MACOS=1)
endif()
