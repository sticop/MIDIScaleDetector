#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <map>

namespace MIDIScaleDetector {

// MIDI Event Types
enum class EventType {
    NoteOn,
    NoteOff,
    ControlChange,
    ProgramChange,
    Tempo,
    TimeSignature,
    KeySignature,
    Unknown
};

// MIDI Event Structure
struct MIDIEvent {
    uint32_t tick;          // Time in MIDI ticks
    double timestamp;       // Time in seconds
    EventType type;
    uint8_t channel;
    uint8_t note;           // MIDI note number (0-127)
    uint8_t velocity;
    uint8_t controller;
    uint8_t value;
    
    MIDIEvent() : tick(0), timestamp(0.0), type(EventType::Unknown),
                  channel(0), note(0), velocity(0), controller(0), value(0) {}
};

// MIDI Track
struct MIDITrack {
    std::string name;
    std::vector<MIDIEvent> events;
    int channel;
    
    MIDITrack() : channel(-1) {}
};

// MIDI File Header
struct MIDIHeader {
    uint16_t format;        // 0, 1, or 2
    uint16_t trackCount;
    uint16_t division;      // Ticks per quarter note
    
    MIDIHeader() : format(0), trackCount(0), division(480) {}
};

// Complete MIDI File
struct MIDIFile {
    MIDIHeader header;
    std::vector<MIDITrack> tracks;
    double tempo;           // BPM
    std::string filePath;
    
    MIDIFile() : tempo(120.0) {}
    
    // Get all note events across all tracks
    std::vector<MIDIEvent> getAllNoteEvents() const;
    
    // Get note events in time range
    std::vector<MIDIEvent> getNoteEventsInRange(double startTime, double endTime) const;
    
    // Get total duration in seconds
    double getDuration() const;
};

// MIDI Parser Class
class MIDIParser {
public:
    MIDIParser();
    ~MIDIParser();
    
    // Parse MIDI file from path
    bool parse(const std::string& filePath, MIDIFile& midiFile);
    
    // Get last error message
    std::string getLastError() const { return lastError; }
    
private:
    std::string lastError;
    
    // Internal parsing methods
    bool parseHeader(const uint8_t* data, size_t size, MIDIHeader& header, size_t& offset);
    bool parseTrack(const uint8_t* data, size_t size, MIDITrack& track, size_t& offset, uint16_t division);
    uint32_t readVariableLength(const uint8_t* data, size_t& offset);
    uint32_t read32BitValue(const uint8_t* data, size_t& offset);
    uint16_t read16BitValue(const uint8_t* data, size_t& offset);
    uint8_t read8BitValue(const uint8_t* data, size_t& offset);
    
    // Convert MIDI ticks to seconds
    double ticksToSeconds(uint32_t ticks, uint16_t division, double tempo) const;
};

} // namespace MIDIScaleDetector
