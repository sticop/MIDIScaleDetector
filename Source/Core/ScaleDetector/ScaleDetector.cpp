#include "ScaleDetector.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>

namespace MIDIScaleDetector {

// Scale implementation
std::string Scale::getName() const {
    return getRootName() + " " + scaleTypeToString(type);
}

std::string Scale::getRootName() const {
    return noteNameToString(root);
}

bool Scale::containsNote(int midiNote) const {
    int pitchClass = midiNote % 12;
    int rootPitch = static_cast<int>(root);

    for (int interval : intervals) {
        if ((rootPitch + interval) % 12 == pitchClass) {
            return true;
        }
    }

    return false;
}

// ScaleDetector implementation
ScaleDetector::ScaleDetector()
    : minConfidence(0.6),
      weightByDuration(true),
      weightByVelocity(true),
      detectKeyChangesEnabled(true) {

    initializeScaleTemplates();
    initializeKeyProfiles();
}

ScaleDetector::~ScaleDetector() {}

void ScaleDetector::initializeScaleTemplates() {
    // Major modes
    scaleTemplates[ScaleType::Ionian] = {0, 2, 4, 5, 7, 9, 11};        // Major
    scaleTemplates[ScaleType::Dorian] = {0, 2, 3, 5, 7, 9, 10};
    scaleTemplates[ScaleType::Phrygian] = {0, 1, 3, 5, 7, 8, 10};
    scaleTemplates[ScaleType::Lydian] = {0, 2, 4, 6, 7, 9, 11};
    scaleTemplates[ScaleType::Mixolydian] = {0, 2, 4, 5, 7, 9, 10};
    scaleTemplates[ScaleType::Aeolian] = {0, 2, 3, 5, 7, 8, 10};       // Natural Minor
    scaleTemplates[ScaleType::Locrian] = {0, 1, 3, 5, 6, 8, 10};

    // Minor variants
    scaleTemplates[ScaleType::HarmonicMinor] = {0, 2, 3, 5, 7, 8, 11};
    scaleTemplates[ScaleType::MelodicMinor] = {0, 2, 3, 5, 7, 9, 11};

    // Pentatonic
    scaleTemplates[ScaleType::MajorPentatonic] = {0, 2, 4, 7, 9};
    scaleTemplates[ScaleType::MinorPentatonic] = {0, 3, 5, 7, 10};

    // Blues
    scaleTemplates[ScaleType::Blues] = {0, 3, 5, 6, 7, 10};

    // Other
    scaleTemplates[ScaleType::Chromatic] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    scaleTemplates[ScaleType::WholeTone] = {0, 2, 4, 6, 8, 10};
    scaleTemplates[ScaleType::Diminished] = {0, 2, 3, 5, 6, 8, 9, 11};
    scaleTemplates[ScaleType::DiminishedHalfWhole] = {0, 1, 3, 4, 6, 7, 9, 10};
}

void ScaleDetector::initializeKeyProfiles() {
    // Krumhansl-Schmuckler major key profile
    majorProfile = {
        6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88
    };

    // Krumhansl-Schmuckler minor key profile
    minorProfile = {
        6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17
    };
}

HarmonicAnalysis ScaleDetector::analyze(const MIDIFile& midiFile) {
    return analyzeRange(midiFile, 0.0, midiFile.getDuration());
}

HarmonicAnalysis ScaleDetector::analyzeRange(const MIDIFile& midiFile,
                                             double startTime, double endTime) {
    HarmonicAnalysis result;

    // Get note events in range
    std::vector<MIDIEvent> events = midiFile.getNoteEventsInRange(startTime, endTime);

    if (events.empty()) {
        return result;
    }

    // Calculate weighted histogram
    result.noteWeights = calculateWeightedHistogram(events, midiFile);

    // Find primary scale
    result.primaryScale = findBestScale(result.noteWeights);

    // Find alternative interpretations
    result.alternativeScales = findAlternativeScales(result.noteWeights, result.primaryScale);

    // Detect chord progressions
    result.chordProgression = detectChordProgressions(midiFile, result.primaryScale);

    // Detect key changes if enabled
    if (detectKeyChangesEnabled && (endTime - startTime) > 8.0) { // Only for longer passages
        result.keyChanges = this->detectKeyChanges(midiFile);
    }

    // Calculate statistics
    result.noteDistribution = calculateNoteDistribution(events);
    result.totalNotes = events.size();

    // Calculate average pitch
    double pitchSum = 0.0;
    for (const auto& event : events) {
        if (event.type == EventType::NoteOn) {
            pitchSum += event.note;
        }
    }
    result.averagePitch = pitchSum / result.totalNotes;

    return result;
}

std::array<double, 12> ScaleDetector::buildNoteHistogram(const std::vector<MIDIEvent>& events) {
    std::array<double, 12> histogram;
    histogram.fill(0.0);

    for (const auto& event : events) {
        if (event.type == EventType::NoteOn) {
            int pitchClass = noteToPitchClass(event.note);
            histogram[pitchClass] += 1.0;
        }
    }

    return histogram;
}

std::array<double, 12> ScaleDetector::calculateWeightedHistogram(
    const std::vector<MIDIEvent>& events, const MIDIFile& midiFile) {

    std::array<double, 12> histogram;
    histogram.fill(0.0);

    // Track active notes for duration calculation
    std::map<int, MIDIEvent> activeNotes;

    for (const auto& event : events) {
        int pitchClass = noteToPitchClass(event.note);

        if (event.type == EventType::NoteOn && event.velocity > 0) {
            activeNotes[event.note] = event;
        } else if (event.type == EventType::NoteOff ||
                  (event.type == EventType::NoteOn && event.velocity == 0)) {

            if (activeNotes.find(event.note) != activeNotes.end()) {
                MIDIEvent noteOn = activeNotes[event.note];

                double weight = 1.0;

                // Weight by duration
                if (weightByDuration) {
                    double duration = event.timestamp - noteOn.timestamp;
                    weight *= duration;
                }

                // Weight by velocity
                if (weightByVelocity) {
                    weight *= (noteOn.velocity / 127.0);
                }

                histogram[pitchClass] += weight;
                activeNotes.erase(event.note);
            }
        }
    }

    // Handle notes still active at end
    for (const auto& pair : activeNotes) {
        int pitchClass = noteToPitchClass(pair.first);
        double weight = 1.0;

        if (weightByVelocity) {
            weight *= (pair.second.velocity / 127.0);
        }

        histogram[pitchClass] += weight;
    }

    normalizeHistogram(histogram);

    return histogram;
}

double ScaleDetector::correlate(const std::array<double, 12>& histogram,
                                const std::array<double, 12>& profile) const {
    // Pearson correlation coefficient
    double meanH = std::accumulate(histogram.begin(), histogram.end(), 0.0) / 12.0;
    double meanP = std::accumulate(profile.begin(), profile.end(), 0.0) / 12.0;

    double numerator = 0.0;
    double denomH = 0.0;
    double denomP = 0.0;

    for (int i = 0; i < 12; ++i) {
        double diffH = histogram[i] - meanH;
        double diffP = profile[i] - meanP;

        numerator += diffH * diffP;
        denomH += diffH * diffH;
        denomP += diffP * diffP;
    }

    if (denomH == 0.0 || denomP == 0.0) {
        return 0.0;
    }

    return numerator / std::sqrt(denomH * denomP);
}

Scale ScaleDetector::findBestScale(const std::array<double, 12>& histogram) {
    Scale bestScale;
    double bestCorrelation = -1.0;

    // Try all 12 root notes with major and minor profiles
    for (int root = 0; root < 12; ++root) {
        // Rotate histogram to test this root
        std::array<double, 12> rotatedHistogram;
        for (int i = 0; i < 12; ++i) {
            rotatedHistogram[i] = histogram[(i + root) % 12];
        }

        // Test major
        double majorCorr = correlate(rotatedHistogram, majorProfile);
        if (majorCorr > bestCorrelation) {
            bestCorrelation = majorCorr;
            bestScale.root = intToNoteName(root);
            bestScale.type = ScaleType::Ionian;
            bestScale.intervals = scaleTemplates[ScaleType::Ionian];
            bestScale.confidence = (majorCorr + 1.0) / 2.0; // Normalize to 0-1
        }

        // Test minor
        double minorCorr = correlate(rotatedHistogram, minorProfile);
        if (minorCorr > bestCorrelation) {
            bestCorrelation = minorCorr;
            bestScale.root = intToNoteName(root);
            bestScale.type = ScaleType::Aeolian;
            bestScale.intervals = scaleTemplates[ScaleType::Aeolian];
            bestScale.confidence = (minorCorr + 1.0) / 2.0;
        }
    }

    // Also test other scale types for the detected root
    if (bestScale.type != ScaleType::Unknown) {
        int rootPitch = static_cast<int>(bestScale.root);

        for (const auto& templatePair : scaleTemplates) {
            if (templatePair.first == ScaleType::Ionian ||
                templatePair.first == ScaleType::Aeolian) {
                continue; // Already tested
            }

            // Check how well the histogram matches this scale
            double matchScore = 0.0;
            double totalWeight = 0.0;

            for (int interval : templatePair.second) {
                int pitchClass = (rootPitch + interval) % 12;
                matchScore += histogram[pitchClass];
                totalWeight += 1.0;
            }

            matchScore /= totalWeight;

            // If better match, update
            if (matchScore > bestScale.confidence) {
                bestScale.type = templatePair.first;
                bestScale.intervals = templatePair.second;
                bestScale.confidence = matchScore;
            }
        }
    }

    return bestScale;
}

std::vector<Scale> ScaleDetector::findAlternativeScales(
    const std::array<double, 12>& histogram, const Scale& primaryScale) {

    std::vector<Scale> alternatives;

    // Find scales with correlation above threshold
    for (int root = 0; root < 12; ++root) {
        std::array<double, 12> rotatedHistogram;
        for (int i = 0; i < 12; ++i) {
            rotatedHistogram[i] = histogram[(i + root) % 12];
        }

        double majorCorr = correlate(rotatedHistogram, majorProfile);
        double minorCorr = correlate(rotatedHistogram, minorProfile);

        double majorConf = (majorCorr + 1.0) / 2.0;
        double minorConf = (minorCorr + 1.0) / 2.0;

        // Add if above threshold and not the primary scale
        if (majorConf >= minConfidence &&
            (root != static_cast<int>(primaryScale.root) ||
             ScaleType::Ionian != primaryScale.type)) {

            Scale alt;
            alt.root = intToNoteName(root);
            alt.type = ScaleType::Ionian;
            alt.intervals = scaleTemplates[ScaleType::Ionian];
            alt.confidence = majorConf;
            alternatives.push_back(alt);
        }

        if (minorConf >= minConfidence &&
            (root != static_cast<int>(primaryScale.root) ||
             ScaleType::Aeolian != primaryScale.type)) {

            Scale alt;
            alt.root = intToNoteName(root);
            alt.type = ScaleType::Aeolian;
            alt.intervals = scaleTemplates[ScaleType::Aeolian];
            alt.confidence = minorConf;
            alternatives.push_back(alt);
        }
    }

    // Sort by confidence
    std::sort(alternatives.begin(), alternatives.end(),
             [](const Scale& a, const Scale& b) {
                 return a.confidence > b.confidence;
             });

    // Return top 3
    if (alternatives.size() > 3) {
        alternatives.resize(3);
    }

    return alternatives;
}

std::vector<std::pair<double, Scale>> ScaleDetector::detectKeyChanges(const MIDIFile& midiFile) {
    std::vector<std::pair<double, Scale>> keyChanges;

    double duration = midiFile.getDuration();
    double windowSize = 4.0; // 4 second windows
    double hopSize = 2.0;    // 2 second hop

    Scale previousScale;
    previousScale.type = ScaleType::Unknown;

    for (double time = 0.0; time < duration; time += hopSize) {
        double endTime = std::min(time + windowSize, duration);

        HarmonicAnalysis analysis = analyzeRange(midiFile, time, endTime);

        // Check if scale changed significantly
        if (previousScale.type != ScaleType::Unknown &&
            (analysis.primaryScale.root != previousScale.root ||
             analysis.primaryScale.type != previousScale.type) &&
            analysis.primaryScale.confidence >= minConfidence) {

            keyChanges.push_back({time, analysis.primaryScale});
        }

        previousScale = analysis.primaryScale;
    }

    return keyChanges;
}

std::vector<std::string> ScaleDetector::detectChordProgressions(
    const MIDIFile& midiFile, const Scale& scale) {

    std::vector<std::string> progression;

    double duration = midiFile.getDuration();
    double windowSize = 1.0; // 1 second chord windows

    auto allEvents = midiFile.getAllNoteEvents();

    for (double time = 0.0; time < duration; time += windowSize) {
        std::string chord = analyzeChord(allEvents, time, time + windowSize);
        if (!chord.empty() && (progression.empty() || progression.back() != chord)) {
            progression.push_back(chord);
        }
    }

    return progression;
}

std::string ScaleDetector::analyzeChord(const std::vector<MIDIEvent>& events,
                                       double windowStart, double windowEnd) {

    std::array<int, 12> activeNotes;
    activeNotes.fill(0);

    // Find notes active in this window
    std::map<int, MIDIEvent> noteStates;

    for (const auto& event : events) {
        if (event.timestamp > windowEnd) break;

        if (event.type == EventType::NoteOn && event.velocity > 0) {
            noteStates[event.note] = event;
        } else if (event.type == EventType::NoteOff ||
                  (event.type == EventType::NoteOn && event.velocity == 0)) {
            noteStates.erase(event.note);
        }

        // Check if note is active in window
        if (event.timestamp >= windowStart && event.timestamp <= windowEnd) {
            for (const auto& pair : noteStates) {
                int pitchClass = pair.first % 12;
                activeNotes[pitchClass] = 1;
            }
        }
    }

    // Simple chord detection - identify common triads
    std::vector<int> activePitches;
    for (int i = 0; i < 12; ++i) {
        if (activeNotes[i] > 0) {
            activePitches.push_back(i);
        }
    }

    if (activePitches.size() < 2) {
        return ""; // Not enough notes for a chord
    }

    // Simplified chord naming
    std::ostringstream oss;
    oss << noteNameToString(intToNoteName(activePitches[0]));

    return oss.str();
}

std::map<int, int> ScaleDetector::calculateNoteDistribution(
    const std::vector<MIDIEvent>& events) {

    std::map<int, int> distribution;

    for (const auto& event : events) {
        if (event.type == EventType::NoteOn) {
            distribution[event.note]++;
        }
    }

    return distribution;
}

void ScaleDetector::normalizeHistogram(std::array<double, 12>& histogram) const {
    double sum = std::accumulate(histogram.begin(), histogram.end(), 0.0);

    if (sum > 0.0) {
        for (auto& value : histogram) {
            value /= sum;
        }
    }
}

// Utility functions
std::string scaleTypeToString(ScaleType type) {
    switch (type) {
        case ScaleType::Ionian: return "Major";
        case ScaleType::Dorian: return "Dorian";
        case ScaleType::Phrygian: return "Phrygian";
        case ScaleType::Lydian: return "Lydian";
        case ScaleType::Mixolydian: return "Mixolydian";
        case ScaleType::Aeolian: return "Minor";
        case ScaleType::Locrian: return "Locrian";
        case ScaleType::HarmonicMinor: return "Harmonic Minor";
        case ScaleType::MelodicMinor: return "Melodic Minor";
        case ScaleType::MajorPentatonic: return "Major Pentatonic";
        case ScaleType::MinorPentatonic: return "Minor Pentatonic";
        case ScaleType::Blues: return "Blues";
        case ScaleType::Chromatic: return "Chromatic";
        case ScaleType::WholeTone: return "Whole Tone";
        case ScaleType::Diminished: return "Diminished";
        case ScaleType::DiminishedHalfWhole: return "Diminished Half-Whole";
        default: return "Unknown";
    }
}

std::string noteNameToString(NoteName note) {
    switch (note) {
        case NoteName::C: return "C";
        case NoteName::Db: return "Db";
        case NoteName::D: return "D";
        case NoteName::Eb: return "Eb";
        case NoteName::E: return "E";
        case NoteName::F: return "F";
        case NoteName::Gb: return "Gb";
        case NoteName::G: return "G";
        case NoteName::Ab: return "Ab";
        case NoteName::A: return "A";
        case NoteName::Bb: return "Bb";
        case NoteName::B: return "B";
        default: return "?";
    }
}

NoteName intToNoteName(int pitchClass) {
    return static_cast<NoteName>(pitchClass % 12);
}

} // namespace MIDIScaleDetector
