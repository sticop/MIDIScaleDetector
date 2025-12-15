cmake_minimum_required(VERSION 3.20)

# Test executable
add_executable(MIDIScaleDetectorTests
    BasicTests.cpp
)

# Link with core library
target_link_libraries(MIDIScaleDetectorTests
    PRIVATE
        MIDIScaleDetectorCore
)

# Add test
add_test(
    NAME BasicTests
    COMMAND MIDIScaleDetectorTests
)

# Set test properties
set_tests_properties(BasicTests PROPERTIES
    TIMEOUT 30
)
