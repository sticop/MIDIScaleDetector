# MIDI Scale Detector

Advanced MIDI file management and musical scale detection application for macOS.

## Features

- **Automatic Scale Detection**: Analyzes MIDI files to identify musical scales, keys, and harmonic context
- **MIDI File Browser**: Fast, searchable interface to organize MIDI files by scale, key, BPM, and other attributes
- **Dual Mode Operation**: Works as standalone macOS app and VST3/AU plugin
- **DAW Integration**: Seamlessly integrates with Ableton Live, Logic Pro, and other DAWs
- **Real-time MIDI Processing**: Route and constrain MIDI input based on detected scales
- **Virtual Instrument Support**: Play any VI through the plugin with intelligent scale mapping

## Architecture

### Core Components

1. **MIDI Analysis Engine** - Scale detection algorithm (Scaler-quality)
2. **File Scanner & Indexer** - System-wide MIDI file discovery and cataloging
3. **Database Layer** - SQLite-based storage for metadata and analysis results
4. **Standalone Application** - Native macOS UI using Swift/SwiftUI
5. **Plugin Framework** - VST3/AU implementations using JUCE
6. **MIDI Router** - Real-time MIDI processing and transformation

## Technology Stack

- **Languages**: C++17, Swift 5
- **Frameworks**: 
  - JUCE (audio plugin framework)
  - SwiftUI (macOS UI)
  - CoreMIDI (MIDI handling)
- **Build System**: CMake
- **Database**: SQLite3
- **Audio Plugins**: VST3, AudioUnit

## Project Structure

```
MIDIScaleDetector/
├── Source/
│   ├── Core/              # Shared core logic
│   │   ├── MIDIParser/    # MIDI file parsing
│   │   ├── ScaleDetector/ # Scale detection algorithm
│   │   ├── Database/      # Data persistence
│   │   └── AudioEngine/   # Audio/MIDI processing
│   ├── Standalone/        # macOS application
│   │   └── UI/            # SwiftUI interfaces
│   ├── Plugin/            # VST3/AU plugin
│   │   ├── VST3/
│   │   └── AU/
│   └── Shared/            # Common utilities
├── Tests/                 # Unit and integration tests
├── Resources/             # Assets, presets, etc.
├── Documentation/         # Technical docs
└── Build/                 # Build outputs
```

## Building

### Prerequisites

- macOS 12.0 or later
- Xcode 14+
- CMake 3.20+
- JUCE Framework 7.0+

### Build Instructions

```bash
# Clone and setup
git clone <repository-url>
cd MIDIScaleDetector

# Build with CMake
mkdir build && cd build
cmake ..
cmake --build . --config Release

# Or use Xcode
open MIDIScaleDetector.xcodeproj
```

## Development Roadmap

- [x] Project structure
- [ ] MIDI parsing implementation
- [ ] Scale detection algorithm
- [ ] File indexing system
- [ ] Database schema
- [ ] Standalone UI
- [ ] VST3 plugin
- [ ] AU plugin
- [ ] Real-time MIDI routing
- [ ] DAW testing and optimization

## License

[To be determined]

## Contributors

[To be added]
