cmake_minimum_required(VERSION 3.20)

project(MIDIXplorerCore)

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
add_library(MIDIXplorerCore STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Link libraries
target_link_libraries(MIDIXplorerCore
    PUBLIC
        SQLite::SQLite3
)

# Include directories
target_include_directories(MIDIXplorerCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Compiler features
target_compile_features(MIDIXplorerCore PUBLIC cxx_std_17)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(MIDIXplorerCore PUBLIC MACOS=1)
endif()
