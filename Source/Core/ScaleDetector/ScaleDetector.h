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

// Scale types - comprehensive list like Scaler
enum class ScaleType {
    // Major modes (Church modes)
    Ionian,             // Major
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Aeolian,            // Natural Minor
    Locrian,

    // Minor variants
    HarmonicMinor,
    MelodicMinor,
    NaturalMinor,

    // Melodic Minor modes
    DorianFlat2,        // Phrygian #6 / Javanese
    LydianAugmented,    // Lydian #5
    LydianDominant,     // Lydian b7 / Overtone
    MixolydianFlat6,    // Hindu / Aeolian Dominant
    LocrianNatural2,    // Half-Diminished
    SuperLocrian,       // Altered / Diminished Whole Tone

    // Harmonic Minor modes
    LocrianNatural6,
    IonianAugmented,    // Ionian #5
    DorianSharp4,       // Romanian Minor
    PhrygianDominant,   // Spanish Phrygian / Freygish
    LydianSharp2,
    SuperLocrianDiminished,

    // Harmonic Major modes
    HarmonicMajor,
    DorianFlat5,
    PhrygianFlat4,
    LydianFlat3,
    MixolydianFlat2,
    LydianAugmentedSharp2,
    LocrianDiminished7,

    // Double Harmonic / Byzantine modes
    DoubleHarmonic,     // Byzantine / Arabic / Gypsy Major
    LydianSharp2Sharp6,
    UltraPhrygian,
    HungarianMinor,     // Gypsy Minor
    Oriental,
    IonianAugmentedSharp2,
    LocrianDiminished3Diminished7,

    // Pentatonic scales
    MajorPentatonic,
    MinorPentatonic,
    EgyptianPentatonic,
    BluesMinorPentatonic,
    BluesMajorPentatonic,
    JapanesePentatonic,
    ChinesePentatonic,

    // Blues scales
    Blues,
    MajorBlues,

    // Bebop scales
    BebopDominant,
    BebopMajor,
    BebopMinor,
    BebopDorian,

    // Symmetric scales
    Chromatic,
    WholeTone,
    Diminished,
    DiminishedHalfWhole,
    Augmented,

    // Ethnic / World scales
    HungarianMajor,
    NeapolitanMajor,
    NeapolitanMinor,
    Persian,
    Hirajoshi,
    Iwato,
    Kumoi,
    InSen,
    Mongolian,
    Balinese,
    Pelog,
    Algerian,
    Spanish8Tone,
    Flamenco,
    Jewish,
    Gypsy,
    Romanian,
    Hawaiian,
    Ethiopian,
    Arabic,

    // Jazz scales
    Enigmatic,
    LeadingWholeTone,
    SixToneSymmetric,
    Prometheus,
    PrometheusNeapolitan,
    Tritone,
    TwoSemitoneTritone,

    // Modal variations
    MajorLocrian,
    ArabicMaqam,
    Istrian,
    UkrainianDorian,
    
    Unknown
};

// Scale definition
struct Scale {
    NoteName root;
    ScaleType type;
    std::vector<int> intervals;
    double confidence;

    Scale() : root(NoteName::C), type(ScaleType::Unknown), confidence(0.0) {}

    std::string getName() const;
    std::string getRootName() const;
    bool containsNote(int midiNote) const;
};

// Harmonic analysis result
struct HarmonicAnalysis {
    Scale primaryScale;
    std::vector<Scale> alternativeScales;
    std::array<double, 12> noteWeights;
    std::vector<std::string> chordProgression;
    std::vector<std::pair<double, Scale>> keyChanges;
    int totalNotes;
    double averagePitch;
    std::map<int, int> noteDistribution;

    HarmonicAnalysis() : totalNotes(0), averagePitch(0.0) {
        noteWeights.fill(0.0);
    }
};

// Scale Detection Engine
class ScaleDetector {
public:
    ScaleDetector();
    ~ScaleDetector();

    HarmonicAnalysis analyze(const MIDIFile& midiFile);
    HarmonicAnalysis analyzeRange(const MIDIFile& midiFile, double startTime, double endTime);

    void setMinConfidenceThreshold(double threshold) { minConfidence = threshold; }
    void setWeightByDuration(bool enabled) { weightByDuration = enabled; }
    void setWeightByVelocity(bool enabled) { weightByVelocity = enabled; }
    void setDetectKeyChanges(bool enabled) { detectKeyChangesEnabled = enabled; }

private:
    double minConfidence;
    bool weightByDuration;
    bool weightByVelocity;
    bool detectKeyChangesEnabled;

    std::map<ScaleType, std::vector<int>> scaleTemplates;
    std::array<double, 12> majorProfile;
    std::array<double, 12> minorProfile;

    void initializeScaleTemplates();
    void initializeKeyProfiles();
    std::array<double, 12> buildNoteHistogram(const std::vector<MIDIEvent>& events);
    std::array<double, 12> calculateWeightedHistogram(const std::vector<MIDIEvent>& events,
                                                       const MIDIFile& midiFile);
    double correlate(const std::array<double, 12>& histogram,
                    const std::array<double, 12>& profile) const;
    Scale findBestScale(const std::array<double, 12>& histogram);
    std::vector<Scale> findAlternativeScales(const std::array<double, 12>& histogram,
                                             const Scale& primaryScale);
    std::vector<std::pair<double, Scale>> detectKeyChanges(const MIDIFile& midiFile);
    std::vector<std::string> detectChordProgressions(const MIDIFile& midiFile,
                                                     const Scale& scale);
    std::string analyzeChord(const std::vector<MIDIEvent>& events,
                            double windowStart, double windowEnd);
    std::map<int, int> calculateNoteDistribution(const std::vector<MIDIEvent>& events);
    void normalizeHistogram(std::array<double, 12>& histogram) const;
    int noteToPitchClass(int midiNote) const { return midiNote % 12; }
};

std::string scaleTypeToString(ScaleType type);
std::string noteNameToString(NoteName note);
NoteName intToNoteName(int pitchClass);

} // namespace MIDIScaleDetector
