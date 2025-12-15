# MIDI Scale Detector - Technical Architecture

## Overview

This document describes the technical architecture of the MIDI Scale Detector system.

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    User Interfaces                           │
├─────────────────────┬───────────────────────────────────────┤
│  Standalone macOS   │     VST3/AU Plugin                    │
│  App (SwiftUI)      │     (JUCE)                            │
└──────────┬──────────┴────────────┬──────────────────────────┘
           │                        │
           └────────────┬───────────┘
                        │
           ┌────────────▼────────────┐
           │   C++ Bridge Layer       │
           └────────────┬────────────┘
                        │
      ┌─────────────────┼─────────────────┐
      │                 │                 │
┌─────▼─────┐  ┌───────▼────────┐  ┌────▼─────┐
│  MIDI     │  │ Scale          │  │ File     │
│  Parser   │  │ Detector       │  │ Scanner  │
└─────┬─────┘  └───────┬────────┘  └────┬─────┘
      │                │                 │
      └────────────────┼─────────────────┘
                       │
              ┌────────▼─────────┐
              │  SQLite Database │
              └──────────────────┘
```

## Core Components

### 1. MIDI Parser (`MIDIParser.h/cpp`)

**Purpose**: Parse standard MIDI files (SMF) and extract musical events.

**Key Features**:

- Supports MIDI Format 0, 1, and 2
- Extracts note on/off, tempo, time signature events
- Converts MIDI ticks to absolute time
- Handles variable-length encoding
- Meta event parsing

**Data Structures**:

```cpp
struct MIDIEvent {
    uint32_t tick;
    double timestamp;
    EventType type;
    uint8_t note, velocity, channel;
}

struct MIDIFile {
    MIDIHeader header;
    vector<MIDITrack> tracks;
    double tempo;
}
```

**Algorithm**:

1. Read file header (MThd chunk)
2. Parse track headers (MTrk chunks)
3. Decode variable-length delta times
4. Parse MIDI events with running status
5. Convert ticks to seconds using tempo

### 2. Scale Detector (`ScaleDetector.h/cpp`)

**Purpose**: Analyze MIDI note data to determine musical scale and key.

**Algorithm**: Krumhansl-Schmuckler Key-Finding

**Process**:

1. **Build Note Histogram**

   - Count note occurrences weighted by duration and velocity
   - Reduce to 12-tone pitch class histogram
   - Normalize to probability distribution

2. **Correlate with Key Profiles**

   ```
   For each of 24 keys (12 major + 12 minor):
       Rotate histogram to test root
       Calculate Pearson correlation with major/minor profile
       Store correlation score
   ```

3. **Select Best Match**

   - Key with highest correlation is primary key
   - Confidence = normalized correlation coefficient

4. **Test Alternative Scales**
   - Test modes (Dorian, Phrygian, etc.)
   - Test pentatonic, blues scales
   - Return alternatives above confidence threshold

**Key Profiles** (Krumhansl-Schmuckler):

- Major: [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]
- Minor: [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]

**Accuracy Factors**:

- More notes = higher confidence
- Tonal music = better results
- Atonal/chromatic music = lower confidence
- Duration weighting improves accuracy

### 3. File Scanner (`FileScanner.h/cpp`)

**Purpose**: Recursively scan filesystem for MIDI files and manage indexing.

**Features**:

- Multi-threaded scanning
- Incremental updates (only scan modified files)
- Exclusion patterns
- Progress callbacks
- Error handling

**Workflow**:

```
1. Recursively traverse directories
2. Filter for .mid/.midi files
3. Check if file exists in database
4. If new or modified:
   - Parse MIDI file
   - Analyze with ScaleDetector
   - Create database entry
   - Store in database
5. Report progress to UI
```

### 4. Database (`Database.h/cpp`)

**Purpose**: Persistent storage of MIDI file metadata and analysis results.

**Schema**:

```sql
CREATE TABLE midi_files (
    id INTEGER PRIMARY KEY,
    file_path TEXT UNIQUE,
    file_name TEXT,
    file_size INTEGER,
    last_modified INTEGER,
    detected_key TEXT,
    detected_scale TEXT,
    confidence REAL,
    tempo REAL,
    duration REAL,
    total_notes INTEGER,
    average_pitch REAL,
    chord_progression TEXT,
    date_added INTEGER,
    date_analyzed INTEGER
);

CREATE INDEX idx_key ON midi_files(detected_key);
CREATE INDEX idx_scale ON midi_files(detected_scale);
```

**Operations**:

- Insert/Update/Delete files
- Search with multiple criteria
- Aggregate statistics (key distribution, etc.)
- Transaction support

### 5. Plugin Architecture (`MIDIScalePlugin.h/cpp`)

**Purpose**: Real-time MIDI processing as VST3/AU plugin.

**JUCE Framework Integration**:

- Inherits from `juce::AudioProcessor`
- Implements MIDI effect interface
- Provides parameter automation
- State save/restore

**Processing Modes**:

1. **Pass Through**: No modification
2. **Constrain**: Snap notes to detected scale
3. **Harmonize**: Add parallel harmony
4. **Arpeggiate**: Generate arpeggios

**Real-time Pipeline**:

```
MIDI Input → Scale Detection → Transformation → MIDI Output
```

**Processing Block**:

```cpp
void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) {
    for each MIDI message:
        if (noteOn):
            transformed_note = applyTransformation(note)
            output transformed_note
}
```

### 6. Standalone UI (`ContentView.swift`)

**Purpose**: Native macOS application interface.

**Technology**: SwiftUI + AppKit

**Architecture**:

- MVVM pattern
- `@StateObject` for app state
- `@EnvironmentObject` for dependency injection
- Combine for reactive updates

**Views**:

- **Sidebar**: Navigation by key/scale
- **File List**: Table view with sorting/filtering
- **Detail Panel**: Comprehensive file analysis
- **Settings**: User preferences

**C++ Bridge**:

- Swift → C++ interop for core functionality
- Objective-C++ wrapper layer
- Async callbacks for long operations

## Data Flow

### Scanning Workflow

```
User triggers scan
    ↓
FileScanner.startScan()
    ↓
Traverse directories
    ↓
For each MIDI file:
    ↓
MIDIParser.parse() → MIDIFile
    ↓
ScaleDetector.analyze() → HarmonicAnalysis
    ↓
Database.addFile() → Store results
    ↓
Progress callback → Update UI
```

### Plugin Real-time Processing

```
DAW sends MIDI buffer
    ↓
Plugin.processBlock()
    ↓
For each note event:
    ↓
Apply current scale constraints
    ↓
Transform based on mode
    ↓
Output modified MIDI
    ↓
DAW receives processed MIDI
```

## Performance Considerations

### Optimization Strategies

1. **MIDI Parsing**:

   - Memory-mapped file I/O
   - Single-pass parsing
   - Efficient variable-length decoding

2. **Scale Detection**:

   - Pre-computed key profiles
   - Fast correlation algorithm
   - Cached results

3. **Database**:

   - Indexed columns for common queries
   - Prepared statements
   - Batch operations

4. **Real-time Processing**:
   - Lock-free MIDI buffer manipulation
   - Minimal allocations in audio thread
   - Pre-calculated scale lookups

### Memory Usage

- **MIDI File**: ~10-100KB typical
- **Analysis Data**: ~2KB per file
- **Database**: ~5KB per indexed file
- **Plugin Instance**: ~100KB

### Latency

- **Parsing**: <10ms for typical file
- **Analysis**: 10-50ms depending on length
- **Plugin Processing**: <1ms per block
- **Database Query**: <5ms

## Threading Model

### Standalone App

- **Main Thread**: UI updates, user interaction
- **Background Queue**: File scanning, analysis
- **Database Thread**: SQLite operations

### Plugin

- **Audio Thread**: MIDI processing (real-time)
- **Message Thread**: UI updates, non-real-time operations

## Extension Points

### Adding New Scales

1. Add scale type to `ScaleType` enum
2. Define interval pattern in `initializeScaleTemplates()`
3. Optional: Add specialized detection logic

### Adding New Transform Modes

1. Add mode to `TransformMode` enum
2. Implement transformation function
3. Add to `processBlock()` switch statement

### Custom Analysis

Extend `HarmonicAnalysis` with:

- Additional metrics
- Specialized detectors
- Custom visualizations

## Dependencies

### Core Libraries

- **C++ Standard Library**: 17
- **SQLite3**: 3.x
- **Filesystem**: C++17 `<filesystem>`

### Plugin Framework

- **JUCE**: 7.0+
  - `juce_audio_processors`
  - `juce_audio_utils`
  - `juce_data_structures`

### macOS Frameworks

- **SwiftUI**: macOS 12+
- **Combine**: Reactive programming
- **AppKit**: Native UI components

## Build System

**CMake** configuration:

- Modular target structure
- Platform-specific optimizations
- Optional components
- Test integration

**Targets**:

- `MIDIScaleDetectorCore`: Static library
- `MIDIScaleDetectorPlugin`: VST3/AU plugin
- `MIDIScaleDetectorApp`: Standalone app (Xcode)

## Testing Strategy

### Unit Tests

- MIDI parser validation
- Scale detection accuracy
- Database operations
- Edge cases

### Integration Tests

- Full scan workflow
- Plugin in DAW context
- UI interaction scenarios

### Performance Tests

- Large file collections
- Real-time processing latency
- Memory usage under load

## Future Enhancements

### Planned Features

1. **Machine Learning**:

   - Neural network for scale detection
   - Training on large MIDI corpus
   - Improved accuracy for complex harmony

2. **Cloud Sync**:

   - iCloud database sync
   - Shared libraries
   - Collaborative features

3. **Advanced Analysis**:

   - Modulation detection
   - Chord progression analysis
   - Style classification

4. **Export Formats**:
   - MusicXML export
   - Scale annotations
   - DAW-specific formats

## References

- Krumhansl, C. L., & Kessler, E. J. (1982). Tracing the dynamic changes in perceived tonal organization
- MIDI 1.0 Specification
- JUCE Framework Documentation
- VST3 SDK Documentation
- Audio Unit Programming Guide
