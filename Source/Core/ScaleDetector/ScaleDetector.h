#pragma once

#include <vector>
#include <string>
#include <map>
#include <array>
#include "../MIDIParser/MIDIParser.h"

namespace MIDIScaleDetector {

// Musical note names
enum class NoteName {
    C = 0, Db, D, Eb, E, F, Gb, G, Ab, A, Bb, B
};

// Scale types (modes)
enum class ScaleType {
    // Major modes
    Ionian,         // Major
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Aeolian,        // Natural Minor
    Locrian,
    
    // Minor variants
    HarmonicMinor,
    MelodicMinor,
    
    // Pentatonic
    MajorPentatonic,
    MinorPentatonic,
    
    // Blues
    Blues,
    
    // Other common scales
    Chromatic,
    WholeTone,
    Diminished,
    DiminishedHalfWhole,
    
    Unknown
};

// Scale definition
struct Scale {
    NoteName root;
    ScaleType type;
    std::vector<int> intervals;  // Semitone intervals from root
    double confidence;            // 0.0 to 1.0
    
    Scale() : root(NoteName::C), type(ScaleType::Unknown), confidence(0.0) {}
    
    // Get scale name as string
    std::string getName() const;
    
    // Get root note name as string
    std::string getRootName() const;
    
    // Check if note is in scale
    bool containsNote(int midiNote) const;
};

// Harmonic analysis result
struct HarmonicAnalysis {
    Scale primaryScale;
    std::vector<Scale> alternativeScales;  // Other possible interpretations
    
    // Note histogram (0-11 for C-B)
    std::array<double, 12> noteWeights;
    
    // Chord progressions detected
    std::vector<std::string> chordProgression;
    
    // Key changes (if any)
    std::vector<std::pair<double, Scale>> keyChanges;  // timestamp, new scale
    
    // Statistical info
    int totalNotes;
    double averagePitch;
    std::map<int, int> noteDistribution;  // MIDI note -> count
    
    HarmonicAnalysis() : totalNotes(0), averagePitch(0.0) {
        noteWeights.fill(0.0);
    }
};

// Scale Detection Engine
class ScaleDetector {
public:
    ScaleDetector();
    ~ScaleDetector();
    
    // Analyze a MIDI file and detect its scale
    HarmonicAnalysis analyze(const MIDIFile& midiFile);
    
    // Analyze a specific time range
    HarmonicAnalysis analyzeRange(const MIDIFile& midiFile, double startTime, double endTime);
    
    // Configuration
    void setMinConfidenceThreshold(double threshold) { minConfidence = threshold; }
    void setWeightByDuration(bool enabled) { weightByDuration = enabled; }
    void setWeightByVelocity(bool enabled) { weightByVelocity = enabled; }
    void setDetectKeyChanges(bool enabled) { detectKeyChanges = enabled; }
    
private:
    double minConfidence;
    bool weightByDuration;
    bool weightByVelocity;
    bool detectKeyChanges;
    
    // Scale templates (interval patterns)
    std::map<ScaleType, std::vector<int>> scaleTemplates;
    
    // Krumhansl-Schmuckler key-finding weights
    std::array<double, 12> majorProfile;
    std::array<double, 12> minorProfile;
    
    // Initialize scale templates
    void initializeScaleTemplates();
    
    // Initialize key profiles
    void initializeKeyProfiles();
    
    // Build note histogram from events
    std::array<double, 12> buildNoteHistogram(const std::vector<MIDIEvent>& events);
    
    // Calculate note weights with duration and velocity
    std::array<double, 12> calculateWeightedHistogram(const std::vector<MIDIEvent>& events,
                                                       const MIDIFile& midiFile);
    
    // Correlate histogram with scale profile
    double correlate(const std::array<double, 12>& histogram,
                    const std::array<double, 12>& profile) const;
    
    // Find best matching scale
    Scale findBestScale(const std::array<double, 12>& histogram);
    
    // Find alternative scales
    std::vector<Scale> findAlternativeScales(const std::array<double, 12>& histogram,
                                             const Scale& primaryScale);
    
    // Detect key changes over time
    std::vector<std::pair<double, Scale>> detectKeyChanges(const MIDIFile& midiFile);
    
    // Detect chord progressions
    std::vector<std::string> detectChordProgressions(const MIDIFile& midiFile,
                                                     const Scale& scale);
    
    // Analyze chord at specific time window
    std::string analyzeChord(const std::vector<MIDIEvent>& events,
                            double windowStart, double windowEnd);
    
    // Calculate note distribution
    std::map<int, int> calculateNoteDistribution(const std::vector<MIDIEvent>& events);
    
    // Normalize histogram
    void normalizeHistogram(std::array<double, 12>& histogram) const;
    
    // Convert MIDI note to pitch class (0-11)
    int noteToPitchClass(int midiNote) const { return midiNote % 12; }
};

// Utility functions
std::string scaleTypeToString(ScaleType type);
std::string noteNameToString(NoteName note);
NoteName intToNoteName(int pitchClass);

} // namespace MIDIScaleDetector
