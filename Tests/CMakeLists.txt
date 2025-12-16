cmake_minimum_required(VERSION 3.20)

# Test executable
add_executable(MIDIXplorerTests
    BasicTests.cpp
)

# Link with core library
target_link_libraries(MIDIXplorerTests
    PRIVATE
        MIDIXplorerCore
)

# Add test
add_test(
    NAME BasicTests
    COMMAND MIDIXplorerTests
)

# Set test properties
set_tests_properties(BasicTests PROPERTIES
    TIMEOUT 30
)
