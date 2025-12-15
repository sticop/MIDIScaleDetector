#!/bin/bash

# MIDI Scale Detector - Setup Script
# This script automates the initial setup process

set -e

echo "========================================="
echo "MIDI Scale Detector - Setup Script"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo -e "${RED}Error: This script is designed for macOS only${NC}"
    exit 1
fi

echo "Checking prerequisites..."
echo ""

# Check for Xcode Command Line Tools
if ! xcode-select -p &> /dev/null; then
    echo -e "${YELLOW}Xcode Command Line Tools not found${NC}"
    echo "Installing Xcode Command Line Tools..."
    xcode-select --install
    echo "Please run this script again after installation completes"
    exit 0
else
    echo -e "${GREEN}✓${NC} Xcode Command Line Tools installed"
fi

# Check for Homebrew
if ! command -v brew &> /dev/null; then
    echo -e "${YELLOW}Homebrew not found${NC}"
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo -e "${GREEN}✓${NC} Homebrew installed"
fi

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${YELLOW}CMake not found${NC}"
    echo "Installing CMake..."
    brew install cmake
else
    echo -e "${GREEN}✓${NC} CMake installed ($(cmake --version | head -n1))"
fi

# Check for SQLite3
if ! command -v sqlite3 &> /dev/null; then
    echo -e "${YELLOW}SQLite3 not found${NC}"
    echo "Installing SQLite3..."
    brew install sqlite3
else
    echo -e "${GREEN}✓${NC} SQLite3 installed"
fi

# Check for JUCE
if [ ! -d "JUCE" ]; then
    echo -e "${YELLOW}JUCE framework not found${NC}"
    echo "Cloning JUCE framework..."
    git clone https://github.com/juce-framework/JUCE.git
    echo -e "${GREEN}✓${NC} JUCE framework downloaded"
else
    echo -e "${GREEN}✓${NC} JUCE framework found"
fi

echo ""
echo "========================================="
echo "Building Project"
echo "========================================="
echo ""

# Create build directory
if [ -d "build" ]; then
    echo "Removing old build directory..."
    rm -rf build
fi

mkdir build
cd build

echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
      ..

echo ""
echo "Building (this may take several minutes)..."
cmake --build . --config Release -j$(sysctl -n hw.ncpu)

echo ""
echo "========================================="
echo "Installation"
echo "========================================="
echo ""

# Create plugin directories if they don't exist
mkdir -p ~/Library/Audio/Plug-Ins/VST3
mkdir -p ~/Library/Audio/Plug-Ins/Components

echo "Installing plugins..."

# Check if plugins were built and install them
if [ -d "MIDIScaleDetectorPlugin_artefacts/VST3" ]; then
    cp -r MIDIScaleDetectorPlugin_artefacts/VST3/*.vst3 ~/Library/Audio/Plug-Ins/VST3/
    echo -e "${GREEN}✓${NC} VST3 plugin installed"
fi

if [ -d "MIDIScaleDetectorPlugin_artefacts/AU" ]; then
    cp -r MIDIScaleDetectorPlugin_artefacts/AU/*.component ~/Library/Audio/Plug-Ins/Components/
    echo -e "${GREEN}✓${NC} AU plugin installed"
fi

echo ""
echo "========================================="
echo "Setup Complete!"
echo "========================================="
echo ""
echo "Next steps:"
echo ""
echo "1. To use the standalone application:"
echo "   cd Source/Standalone"
echo "   open MIDIScaleDetector.xcodeproj"
echo ""
echo "2. To use the plugin:"
echo "   - Open your DAW (Ableton, Logic, etc.)"
echo "   - Rescan plugins"
echo "   - Look for 'MIDI Scale Detector'"
echo ""
echo "3. To run tests:"
echo "   cd build"
echo "   ctest"
echo ""
echo "Documentation available in Documentation/ folder"
echo ""
echo -e "${GREEN}Happy music making!${NC}"
echo ""
