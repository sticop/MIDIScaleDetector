# MIDI Xplorer - Build Instructions

## Prerequisites

### Required Software

1. **Xcode 14+** (for macOS development)

   ```bash
   xcode-select --install
   ```

2. **CMake 3.20+**

   ```bash
   brew install cmake
   ```

3. **JUCE Framework 7.0+**

   ```bash
   cd MIDIXplorer
   git clone https://github.com/juce-framework/JUCE.git
   ```

4. **SQLite3** (usually included with macOS)
   ```bash
   # Verify installation
   sqlite3 --version
   ```

### Optional Tools

- **Homebrew** (recommended for dependency management)
- **ninja** (faster builds)
  ```bash
  brew install ninja
  ```

## Building the Project

### 1. Clone and Setup

```bash
git clone <repository-url>
cd MIDIXplorer
```

### 2. Download JUCE

If you haven't already:

```bash
git clone https://github.com/juce-framework/JUCE.git
```

### 3. Create Build Directory

```bash
mkdir build
cd build
```

### 4. Configure with CMake

```bash
# Standard configuration
cmake ..

# Or with Ninja for faster builds
cmake -G Ninja ..

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### 5. Build

```bash
# Build everything
cmake --build . --config Release

# Build specific targets
cmake --build . --target MIDIXplorerCore
cmake --build . --target MIDIXplorerPlugin
```

### 6. Build Options

You can disable specific components:

```bash
# Build without standalone app
cmake -DBUILD_STANDALONE=OFF ..

# Build without VST3
cmake -DBUILD_VST3=OFF ..

# Build without AU
cmake -DBUILD_AU=OFF ..

# Build without tests
cmake -DBUILD_TESTS=OFF ..
```

## Building the Standalone App

The standalone macOS application uses SwiftUI and requires Xcode:

```bash
cd Source/Standalone
xcodebuild -scheme MIDIXplorer -configuration Release
```

Or open in Xcode:

```bash
open Source/Standalone/MIDIXplorer.xcodeproj
```

## Installing Plugins

### VST3 Plugin

After building, the VST3 plugin will be automatically copied to:

```
~/Library/Audio/Plug-Ins/VST3/MIDI Xplorer.vst3
```

### Audio Unit Plugin

The AU plugin will be copied to:

```
~/Library/Audio/Plug-Ins/Components/MIDI Xplorer.component
```

### Manual Installation

If automatic installation fails:

```bash
# VST3
cp -r build/MIDIXplorerPlugin_artefacts/VST3/*.vst3 ~/Library/Audio/Plug-Ins/VST3/

# AU
cp -r build/MIDIXplorerPlugin_artefacts/AU/*.component ~/Library/Audio/Plug-Ins/Components/
```

### Verify Installation

```bash
# List installed plugins
ls ~/Library/Audio/Plug-Ins/VST3/
ls ~/Library/Audio/Plug-Ins/Components/

# Validate AU
auval -v aufx Msdp Msdt
```

## Testing in DAWs

### Ableton Live

1. Open Ableton Live
2. Go to Preferences → Plug-Ins
3. Click "Rescan"
4. Create a MIDI track
5. Add "MIDI Xplorer" as a MIDI effect

### Logic Pro

1. Open Logic Pro
2. Create a Software Instrument track
3. Click on "MIDI FX" slot
4. Select "Audio Units" → "MIDIXplorer" → "MIDI Xplorer"

## Running Tests

```bash
cd build
ctest --output-on-failure

# Or run specific test
./Tests/MIDIParserTests
./Tests/ScaleDetectorTests
```

## Development Workflow

### Quick Rebuild

```bash
cd build
cmake --build . --config Release -j8
```

### Clean Build

```bash
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Debug Build with Xcode

```bash
cmake -G Xcode ..
open MIDIXplorer.xcodeproj
```

## Troubleshooting

### JUCE Not Found

```bash
# Make sure JUCE is in the project root
ls JUCE/CMakeLists.txt

# Or specify JUCE path
cmake -DJUCE_DIR=/path/to/JUCE ..
```

### SQLite3 Issues

```bash
# Install via Homebrew if needed
brew install sqlite3

# Specify SQLite path
cmake -DSQLite3_INCLUDE_DIR=/usr/local/include ..
```

### Plugin Not Showing in DAW

1. **Rescan plugins** in your DAW
2. **Check installation location**:
   ```bash
   ls ~/Library/Audio/Plug-Ins/VST3/
   ls ~/Library/Audio/Plug-Ins/Components/
   ```
3. **Validate AU plugin**:
   ```bash
   auval -v aufx Msdp Msdt
   ```
4. **Check plugin architecture** (should be universal binary):
   ```bash
   lipo -info ~/Library/Audio/Plug-Ins/VST3/MIDI\ Scale\ Detector.vst3/Contents/MacOS/MIDI\ Scale\ Detector
   ```

### Codesigning (for Distribution)

```bash
# Sign the plugin
codesign --force --sign "Developer ID Application: Your Name" \
  ~/Library/Audio/Plug-Ins/VST3/MIDI\ Scale\ Detector.vst3

# Verify signature
codesign --verify --verbose ~/Library/Audio/Plug-Ins/VST3/MIDI\ Scale\ Detector.vst3
```

## Performance Optimization

### Release Build Optimizations

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native" \
      ..
```

### Link-Time Optimization

```bash
cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ..
```

## Cross-Compilation

### Universal Binary (x86_64 + ARM64)

```bash
cmake -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..
```

## Continuous Integration

See `.github/workflows/build.yml` for automated build configuration.

## Additional Resources

- [JUCE Documentation](https://docs.juce.com)
- [CMake Documentation](https://cmake.org/documentation/)
- [Apple Audio Unit Documentation](https://developer.apple.com/documentation/audiounit)

## Support

For issues and questions:

- GitHub Issues: [Link to repository issues]
- Documentation: `Documentation/` folder
- Email: [Support email]
