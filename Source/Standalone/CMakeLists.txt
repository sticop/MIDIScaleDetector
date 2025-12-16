cmake_minimum_required(VERSION 3.20)

# For now, standalone app is built separately via Xcode
# This is a placeholder to satisfy CMake

message(STATUS "Standalone app is built via Xcode project in Source/Standalone/")
message(STATUS "Open Source/Standalone/ in Xcode to build the standalone app")

# Create a dummy target
add_custom_target(MIDIXplorerStandalone
    COMMENT "Standalone app is built via Xcode - see Source/Standalone/"
)
