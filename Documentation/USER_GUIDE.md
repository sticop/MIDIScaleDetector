# MIDI Xplorer - User Guide

## Overview

MIDI Xplorer is an advanced tool for analyzing, organizing, and working with MIDI files based on their musical scale and harmonic content.

## Features

### 1. Automatic Scale Detection

The application analyzes MIDI files to automatically detect:

- **Musical Key** (C, D, E, F, G, A, B and their sharps/flats)
- **Scale Type** (Major, Minor, Dorian, Phrygian, Lydian, etc.)
- **Confidence Level** (How certain the detection is)
- **Tempo/BPM**
- **Chord Progressions**

### 2. MIDI File Browser

A fast, searchable interface to:

- Browse all MIDI files on your system
- Filter by key, scale, tempo, or duration
- Search by filename
- View detailed analysis for each file
- Preview files before opening in your DAW

### 3. DAW Integration

Use as a VST3 or AU plugin in:

- Ableton Live
- Logic Pro
- FL Studio
- Cubase
- Any VST3/AU compatible DAW

## Getting Started

### Standalone Application

#### First Launch

1. **Open MIDI Xplorer**
2. **Scan Your MIDI Files**
   - Click "Scan" button
   - Add folders containing your MIDI files
   - Click "Start Scan"
   - Wait for analysis to complete

#### Browsing Your Library

The main window has three sections:

1. **Sidebar** (left)

   - Navigate by key or scale
   - Access favorites

2. **File List** (center)

   - View all files with their properties
   - Sort by any column
   - Select files to view details

3. **Detail Panel** (right)
   - View comprehensive analysis
   - Preview note distribution
   - Play file preview
   - Export to DAW

#### Searching and Filtering

**Search Bar:**

- Type to search by filename
- Results update in real-time

**Filter Menu:**

- Filter by specific key (e.g., "C", "G#")
- Filter by scale type (e.g., "Major", "Minor")
- Combine filters for precise results

**Sidebar Filtering:**

- Click any key or scale in sidebar
- View only files matching that criteria

### Plugin Usage

#### Installing the Plugin

The plugin is automatically installed during build:

- **VST3**: `~/Library/Audio/Plug-Ins/VST3/`
- **AU**: `~/Library/Audio/Plug-Ins/Components/`

#### Loading in Ableton Live

1. Create a MIDI track
2. In MIDI Effects, add "MIDI Xplorer"
3. The plugin will appear in the device chain
4. Load a MIDI file or play live MIDI

#### Loading in Logic Pro

1. Create a Software Instrument track
2. Click the MIDI FX slot
3. Select Audio Units → MIDIXplorer → MIDI Xplorer
4. The plugin interface will appear

#### Plugin Features

**Transform Modes:**

1. **Off (Pass Through)**

   - MIDI passes unchanged
   - Use for analysis only

2. **Constrain**

   - Snaps notes to detected scale
   - Wrong notes are corrected automatically
   - Great for learning scales

3. **Harmonize**

   - Adds harmony notes based on scale
   - Creates triads automatically
   - Instant chord generation

4. **Arpeggiate**
   - Converts notes to arpeggios
   - Based on detected scale
   - Perfect for melodic patterns

**Parameters:**

- **Root Note**: Manually set the key
- **Scale Type**: Manually set the scale
- **Confidence**: View detection confidence
- **Transform Mode**: Select processing mode

## Workflow Examples

### Example 1: Finding MIDI Files in C Major

1. Click "C" in the Keys section of sidebar
2. In the filter menu, select Scale → "Major"
3. Browse filtered results
4. Select a file to view details
5. Click "Export to DAW" to use it

### Example 2: Correcting Off-Key MIDI in Real-Time

1. Load MIDI Xplorer as plugin in your DAW
2. Create or load a MIDI clip
3. Plugin auto-detects the scale
4. Set Transform Mode to "Constrain"
5. Play the MIDI - wrong notes are corrected
6. Adjust Root Note if detection is incorrect

### Example 3: Creating Harmonies

1. Load plugin on a MIDI track
2. Record a simple melody
3. Set Transform Mode to "Harmonize"
4. Play back - instant harmony added
5. Route to any virtual instrument

### Example 4: Building a Scale Library

1. Scan your entire MIDI collection
2. Sort by "Scale Type" column
3. Create playlists by dragging to favorites
4. Build reference library organized by scale

## Advanced Features

### Key Change Detection

For longer MIDI files, the analyzer can detect key changes:

- View key changes in the detail panel
- See timestamps of modulations
- Understand harmonic structure

### Chord Progression Analysis

The Detail View shows detected chord progressions:

- View common progressions (I-IV-V, etc.)
- Understand harmonic movement
- Learn from analyzed files

### Note Distribution Visualization

The piano roll view shows:

- Which notes are most common
- Note density across octaves
- Visual representation of scale usage

### Statistics

View collection statistics:

- Total files analyzed
- Key distribution across library
- Scale type popularity
- Average confidence scores

## Tips and Tricks

### Improving Detection Accuracy

1. **Clean MIDI Files**: Files with cleaner note data analyze better
2. **Longer Passages**: More notes = higher confidence
3. **Manual Override**: You can manually set key/scale if needed
4. **Re-analyze**: Rescan modified files to update analysis

### Organizing Your Library

1. **Use Favorites**: Star important files
2. **Descriptive Names**: Rename files with key info
3. **Folder Structure**: Organize source files by genre/project
4. **Regular Scans**: Rescan when adding new files

### Real-Time Performance

For best plugin performance:

1. Use lower latency buffer sizes
2. Disable features you're not using
3. Freeze tracks when not editing
4. Process in place vs. real-time

### Learning Music Theory

Use the tool to:

1. Analyze famous songs
2. Compare scales visually
3. Experiment with transformations
4. Study chord progressions

## Keyboard Shortcuts

**Standalone App:**

- `⌘S` - Start new scan
- `⌘I` - Import single MIDI file
- `⌘F` - Focus search field
- `Space` - Preview selected file
- `⌘R` - Refresh library

## Troubleshooting

### "No Files Found" After Scan

- Check that folders contain .mid or .midi files
- Verify file permissions
- Try adding folders individually
- Check excluded paths in settings

### Low Confidence Scores

- File may be atonal or chromatic
- Try longer MIDI passages
- Check for errors in MIDI data
- May be intentionally complex harmony

### Plugin Not Appearing in DAW

1. Rescan plugins in DAW preferences
2. Check installation location
3. Restart DAW
4. See BUILD.md for validation steps

### Slow Performance

- Reduce number of analyzed files
- Close other applications
- Check Activity Monitor
- Rebuild database (Settings → Database → Rebuild)

## Settings

### General Settings

- **Database Location**: Where MIDI analysis data is stored
- **Auto-scan**: Automatically scan new files
- **Preview Duration**: How long to preview files

### Scanning Settings

- **Recursive Scan**: Include subfolders
- **Rescan Modified**: Re-analyze changed files
- **File Extensions**: Which extensions to scan
- **Excluded Folders**: Folders to skip

### Plugin Settings

- **Default Transform Mode**: Mode when loading plugin
- **Auto-detect**: Automatically detect scales
- **Latency Compensation**: Enable for better timing

## FAQ

**Q: How accurate is the scale detection?**
A: Very accurate for tonal music (95%+ confidence). Less accurate for atonal or highly chromatic music.

**Q: Can it detect exotic scales?**
A: Yes, including modes, pentatonic, blues, whole tone, and diminished scales.

**Q: Does it work with live MIDI input?**
A: Yes, in plugin mode it can analyze and transform live input.

**Q: Can I batch export files?**
A: Currently single file export. Batch features coming in future update.

**Q: Does it modify original MIDI files?**
A: No, analysis is non-destructive. Original files are never changed.

**Q: What about microtonal music?**
A: Currently supports standard 12-tone equal temperament.

## Support and Resources

- **Documentation**: See Documentation/ folder
- **GitHub**: [Repository URL]
- **Issues**: Report bugs on GitHub Issues
- **Email**: [Support email]
- **Updates**: Check for updates regularly

## Credits

Developed using:

- JUCE Framework
- SQLite Database
- SwiftUI (macOS app)
- Krumhansl-Schmuckler key-finding algorithm

## License

[License information]
