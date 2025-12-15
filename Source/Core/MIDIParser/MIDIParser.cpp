#include "MIDIParser.h"
#include <fstream>
#include <algorithm>
#include <cstring>

namespace MIDIScaleDetector {

// MIDIFile methods implementation
std::vector<MIDIEvent> MIDIFile::getAllNoteEvents() const {
    std::vector<MIDIEvent> allEvents;

    for (const auto& track : tracks) {
        for (const auto& event : track.events) {
            if (event.type == EventType::NoteOn || event.type == EventType::NoteOff) {
                allEvents.push_back(event);
            }
        }
    }

    // Sort by timestamp
    std::sort(allEvents.begin(), allEvents.end(),
              [](const MIDIEvent& a, const MIDIEvent& b) {
                  return a.timestamp < b.timestamp;
              });

    return allEvents;
}

std::vector<MIDIEvent> MIDIFile::getNoteEventsInRange(double startTime, double endTime) const {
    auto allEvents = getAllNoteEvents();
    std::vector<MIDIEvent> rangeEvents;

    for (const auto& event : allEvents) {
        if (event.timestamp >= startTime && event.timestamp <= endTime) {
            rangeEvents.push_back(event);
        }
    }

    return rangeEvents;
}

double MIDIFile::getDuration() const {
    double maxTime = 0.0;

    for (const auto& track : tracks) {
        for (const auto& event : track.events) {
            maxTime = std::max(maxTime, event.timestamp);
        }
    }

    return maxTime;
}

// MIDIParser implementation
MIDIParser::MIDIParser() : lastError("") {}

MIDIParser::~MIDIParser() {}

bool MIDIParser::parse(const std::string& filePath, MIDIFile& midiFile) {
    // Read entire file into memory
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        lastError = "Failed to open file: " + filePath;
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        lastError = "Failed to read file: " + filePath;
        return false;
    }

    midiFile.filePath = filePath;

    // Parse header
    size_t offset = 0;
    if (!parseHeader(buffer.data(), buffer.size(), midiFile.header, offset)) {
        return false;
    }

    // Parse tracks
    midiFile.tracks.clear();
    midiFile.tracks.reserve(midiFile.header.trackCount);

    double currentTempo = 120.0; // Default tempo

    for (uint16_t i = 0; i < midiFile.header.trackCount; ++i) {
        MIDITrack track;
        if (!parseTrack(buffer.data(), buffer.size(), track, offset, midiFile.header.division)) {
            lastError = "Failed to parse track " + std::to_string(i);
            return false;
        }

        // Extract tempo from first track if available
        if (i == 0) {
            for (const auto& event : track.events) {
                if (event.type == EventType::Tempo) {
                    currentTempo = event.value; // Simplified
                    break;
                }
            }
        }

        midiFile.tracks.push_back(track);
    }

    midiFile.tempo = currentTempo;

    return true;
}

bool MIDIParser::parseHeader(const uint8_t* data, size_t size, MIDIHeader& header, size_t& offset) {
    if (size < 14) {
        lastError = "File too small to contain MIDI header";
        return false;
    }

    // Check MThd marker
    if (std::memcmp(data + offset, "MThd", 4) != 0) {
        lastError = "Invalid MIDI file: Missing MThd marker";
        return false;
    }
    offset += 4;

    // Header length (should be 6)
    uint32_t headerLength = read32BitValue(data, offset);
    if (headerLength != 6) {
        lastError = "Invalid MIDI header length";
        return false;
    }

    // Format type
    header.format = read16BitValue(data, offset);
    if (header.format > 2) {
        lastError = "Unsupported MIDI format: " + std::to_string(header.format);
        return false;
    }

    // Number of tracks
    header.trackCount = read16BitValue(data, offset);

    // Time division
    header.division = read16BitValue(data, offset);

    return true;
}

bool MIDIParser::parseTrack(const uint8_t* data, size_t size, MIDITrack& track,
                            size_t& offset, uint16_t division) {
    if (offset + 8 > size) {
        lastError = "Unexpected end of file while parsing track";
        return false;
    }

    // Check MTrk marker
    if (std::memcmp(data + offset, "MTrk", 4) != 0) {
        lastError = "Invalid track: Missing MTrk marker";
        return false;
    }
    offset += 4;

    // Track length
    uint32_t trackLength = read32BitValue(data, offset);
    size_t trackEnd = offset + trackLength;

    if (trackEnd > size) {
        lastError = "Track length exceeds file size";
        return false;
    }

    uint32_t currentTick = 0;
    double currentTime = 0.0;
    double tempo = 120.0; // Default BPM
    uint8_t runningStatus = 0;

    while (offset < trackEnd) {
        // Read delta time
        uint32_t deltaTime = readVariableLength(data, offset);
        currentTick += deltaTime;
        currentTime += ticksToSeconds(deltaTime, division, tempo);

        // Read event type
        uint8_t status = read8BitValue(data, offset);

        // Handle running status
        if ((status & 0x80) == 0) {
            offset--; // Back up, this is a data byte
            status = runningStatus;
        } else {
            runningStatus = status;
        }

        uint8_t eventType = status & 0xF0;
        uint8_t channel = status & 0x0F;

        MIDIEvent event;
        event.tick = currentTick;
        event.timestamp = currentTime;
        event.channel = channel;

        switch (eventType) {
            case 0x80: // Note Off
                event.type = EventType::NoteOff;
                event.note = read8BitValue(data, offset);
                event.velocity = read8BitValue(data, offset);
                track.events.push_back(event);
                break;

            case 0x90: // Note On
                event.type = EventType::NoteOn;
                event.note = read8BitValue(data, offset);
                event.velocity = read8BitValue(data, offset);
                // Velocity 0 is actually Note Off
                if (event.velocity == 0) {
                    event.type = EventType::NoteOff;
                }
                track.events.push_back(event);
                break;

            case 0xB0: // Control Change
                event.type = EventType::ControlChange;
                event.controller = read8BitValue(data, offset);
                event.value = read8BitValue(data, offset);
                track.events.push_back(event);
                break;

            case 0xC0: // Program Change
                event.type = EventType::ProgramChange;
                event.value = read8BitValue(data, offset);
                track.events.push_back(event);
                break;

            case 0xF0: // System/Meta events
                if (status == 0xFF) {
                    uint8_t metaType = read8BitValue(data, offset);
                    uint32_t metaLength = readVariableLength(data, offset);

                    if (metaType == 0x51 && metaLength == 3) {
                        // Tempo meta event
                        uint32_t microsecondsPerQuarter =
                            (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
                        tempo = 60000000.0 / microsecondsPerQuarter;
                    } else if (metaType == 0x03) {
                        // Track name
                        track.name = std::string(reinterpret_cast<const char*>(data + offset), metaLength);
                    }

                    offset += metaLength;
                } else {
                    // SysEx - skip for now
                    uint32_t sysexLength = readVariableLength(data, offset);
                    offset += sysexLength;
                }
                break;

            default:
                // Other event types - skip data bytes
                if (eventType >= 0x80 && eventType < 0xF0) {
                    offset++; // Skip one data byte
                    if (eventType != 0xC0 && eventType != 0xD0) {
                        offset++; // Skip second data byte for most events
                    }
                }
                break;
        }
    }

    return true;
}

uint32_t MIDIParser::readVariableLength(const uint8_t* data, size_t& offset) {
    uint32_t value = 0;
    uint8_t byte;

    do {
        byte = data[offset++];
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);

    return value;
}

uint32_t MIDIParser::read32BitValue(const uint8_t* data, size_t& offset) {
    uint32_t value = (data[offset] << 24) | (data[offset + 1] << 16) |
                     (data[offset + 2] << 8) | data[offset + 3];
    offset += 4;
    return value;
}

uint16_t MIDIParser::read16BitValue(const uint8_t* data, size_t& offset) {
    uint16_t value = (data[offset] << 8) | data[offset + 1];
    offset += 2;
    return value;
}

uint8_t MIDIParser::read8BitValue(const uint8_t* data, size_t& offset) {
    return data[offset++];
}

double MIDIParser::ticksToSeconds(uint32_t ticks, uint16_t division, double tempo) const {
    // tempo is in BPM
    double secondsPerBeat = 60.0 / tempo;
    double secondsPerTick = secondsPerBeat / division;
    return ticks * secondsPerTick;
}

} // namespace MIDIScaleDetector
