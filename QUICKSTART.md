# MIDI Xplorer - Quick Start Guide

Get up and running in 5 minutes!

## Prerequisites

- macOS 12.0 or later
- 2GB free disk space
- Internet connection (for downloading dependencies)

## Installation

### Option 1: Automated Setup (Recommended)

```bash
# Clone the repository
git clone <repository-url>
cd MIDIXplorer

# Run setup script
chmod +x setup.sh
./setup.sh
```

The script will:

1. Install required tools (CMake, SQLite, etc.)
2. Download JUCE framework
3. Build the project
4. Install plugins to system directories

### Option 2: Manual Setup

See [BUILD.md](BUILD.md) for detailed instructions.

## First Steps

### 1. Using the Standalone Application

```bash
# Open in Xcode
cd Source/Standalone
open MIDIXplorer.xcodeproj

# Build and run (⌘R)
```

**On first launch:**

1. Click the **Scan** button
2. Add your MIDI folders (e.g., `~/Music/MIDI Files`)
3. Click **Start Scan**
4. Wait for analysis to complete

### 2. Using the Plugin

**In Ableton Live:**

1. Open Ableton Live
2. Preferences → Plug-Ins → Rescan
3. Create a MIDI track
4. Add "MIDI Xplorer" from MIDI Effects
5. Play or load a MIDI clip

**In Logic Pro:**

1. Open Logic Pro
2. Create a Software Instrument track
3. Click MIDI FX slot
4. Choose: Audio Units → MIDIXplorer → MIDI Xplorer

## Basic Usage

### Browsing Your MIDI Library

1. **Search**: Type in the search box to find files by name
2. **Filter**: Use the sidebar to filter by key (C, D, E, etc.) or scale (Major, Minor, etc.)
3. **Sort**: Click column headers to sort by any property
4. **View Details**: Click a file to see full analysis

### Real-Time MIDI Processing

1. Load the plugin in your DAW
2. The plugin will auto-detect the scale of incoming MIDI
3. Choose a transform mode:
   - **Off**: No processing (analysis only)
   - **Constrain**: Snap notes to scale
   - **Harmonize**: Add harmony notes
   - **Arpeggiate**: Create arpeggios

## Examples

### Example 1: Find All MIDI Files in C Major

1. Click **"C"** in the sidebar
2. Select **Scale** → **"Major"** from filter menu
3. Browse results

### Example 2: Correct Off-Key Notes

1. Load plugin on MIDI track
2. Plugin detects key automatically
3. Set Transform Mode to **"Constrain"**
4. Wrong notes are automatically corrected

### Example 3: Add Instant Harmonies

1. Record a simple melody
2. Set Transform Mode to **"Harmonize"**
3. Play back to hear automatic harmony

## Keyboard Shortcuts

- `⌘S` - Start new scan
- `⌘F` - Focus search
- `Space` - Preview file
- `⌘I` - Import file

## Tips

1. **First scan takes longest** - Subsequent scans only process new/changed files
2. **Higher confidence = better detection** - Look for 80%+ confidence scores
3. **Manual override available** - Set key manually if auto-detection is wrong
4. **Works with any virtual instrument** - Use plugin before your favorite synth

## Troubleshooting

**Plugin not showing in DAW?**

```bash
# Verify installation
ls ~/Library/Audio/Plug-Ins/VST3/
ls ~/Library/Audio/Plug-Ins/Components/

# Rescan plugins in your DAW
```

**Database errors?**

```bash
# Reset database
rm ~/Library/Application\ Support/MIDIXplorer/midi_library.db

# Restart app and rescan
```

**Build errors?**

```bash
# Clean and rebuild
rm -rf build
./setup.sh
```

## Next Steps

- Read the [User Guide](USER_GUIDE.md) for detailed features
- Check [Architecture](ARCHITECTURE.md) for technical details
- See [BUILD.md](BUILD.md) for advanced build options

## Getting Help

- **Documentation**: Check the `Documentation/` folder
- **Issues**: Report bugs on GitHub Issues
- **Community**: [Link to community forum/Discord]

## What's Next?

Explore these features:

- Key change detection
- Chord progression analysis
- Collection statistics
- Custom scale libraries

Happy music making! 🎵
