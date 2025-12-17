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
    // ===== Major modes (Church modes) =====
    scaleTemplates[ScaleType::Ionian] = {0, 2, 4, 5, 7, 9, 11};           // Major
    scaleTemplates[ScaleType::Dorian] = {0, 2, 3, 5, 7, 9, 10};
    scaleTemplates[ScaleType::Phrygian] = {0, 1, 3, 5, 7, 8, 10};
    scaleTemplates[ScaleType::Lydian] = {0, 2, 4, 6, 7, 9, 11};
    scaleTemplates[ScaleType::Mixolydian] = {0, 2, 4, 5, 7, 9, 10};
    scaleTemplates[ScaleType::Aeolian] = {0, 2, 3, 5, 7, 8, 10};          // Natural Minor
    scaleTemplates[ScaleType::Locrian] = {0, 1, 3, 5, 6, 8, 10};

    // ===== Minor variants =====
    scaleTemplates[ScaleType::HarmonicMinor] = {0, 2, 3, 5, 7, 8, 11};
    scaleTemplates[ScaleType::MelodicMinor] = {0, 2, 3, 5, 7, 9, 11};
    scaleTemplates[ScaleType::NaturalMinor] = {0, 2, 3, 5, 7, 8, 10};

    // ===== Melodic Minor modes =====
    scaleTemplates[ScaleType::DorianFlat2] = {0, 1, 3, 5, 7, 9, 10};      // Phrygian #6 / Javanese
    scaleTemplates[ScaleType::LydianAugmented] = {0, 2, 4, 6, 8, 9, 11};  // Lydian #5
    scaleTemplates[ScaleType::LydianDominant] = {0, 2, 4, 6, 7, 9, 10};   // Lydian b7 / Overtone
    scaleTemplates[ScaleType::MixolydianFlat6] = {0, 2, 4, 5, 7, 8, 10};  // Aeolian Dominant / Hindu
    scaleTemplates[ScaleType::LocrianNatural2] = {0, 2, 3, 5, 6, 8, 10};  // Half-Diminished
    scaleTemplates[ScaleType::SuperLocrian] = {0, 1, 3, 4, 6, 8, 10};     // Altered / Diminished Whole Tone

    // ===== Harmonic Minor modes =====
    scaleTemplates[ScaleType::LocrianNatural6] = {0, 1, 3, 5, 6, 9, 10};
    scaleTemplates[ScaleType::IonianAugmented] = {0, 2, 4, 5, 8, 9, 11};  // Ionian #5
    scaleTemplates[ScaleType::DorianSharp4] = {0, 2, 3, 6, 7, 9, 10};     // Romanian Minor / Ukrainian Dorian
    scaleTemplates[ScaleType::PhrygianDominant] = {0, 1, 4, 5, 7, 8, 10}; // Spanish Phrygian / Freygish
    scaleTemplates[ScaleType::LydianSharp2] = {0, 3, 4, 6, 7, 9, 11};
    scaleTemplates[ScaleType::SuperLocrianDiminished] = {0, 1, 3, 4, 6, 8, 9}; // Ultra Locrian

    // ===== Harmonic Major modes =====
    scaleTemplates[ScaleType::HarmonicMajor] = {0, 2, 4, 5, 7, 8, 11};
    scaleTemplates[ScaleType::DorianFlat5] = {0, 2, 3, 5, 6, 9, 10};
    scaleTemplates[ScaleType::PhrygianFlat4] = {0, 1, 3, 4, 7, 8, 10};
    scaleTemplates[ScaleType::LydianFlat3] = {0, 2, 3, 6, 7, 9, 11};
    scaleTemplates[ScaleType::MixolydianFlat2] = {0, 1, 4, 5, 7, 9, 10};
    scaleTemplates[ScaleType::LydianAugmentedSharp2] = {0, 3, 4, 6, 8, 9, 11};
    scaleTemplates[ScaleType::LocrianDiminished7] = {0, 1, 3, 5, 6, 8, 9};

    // ===== Double Harmonic / Byzantine modes =====
    scaleTemplates[ScaleType::DoubleHarmonic] = {0, 1, 4, 5, 7, 8, 11};   // Byzantine / Arabic / Gypsy Major
    scaleTemplates[ScaleType::LydianSharp2Sharp6] = {0, 3, 4, 6, 7, 10, 11};
    scaleTemplates[ScaleType::UltraPhrygian] = {0, 1, 3, 4, 7, 8, 9};
    scaleTemplates[ScaleType::HungarianMinor] = {0, 2, 3, 6, 7, 8, 11};   // Gypsy Minor
    scaleTemplates[ScaleType::Oriental] = {0, 1, 4, 5, 6, 9, 10};
    scaleTemplates[ScaleType::IonianAugmentedSharp2] = {0, 3, 4, 5, 8, 9, 11};
    scaleTemplates[ScaleType::LocrianDiminished3Diminished7] = {0, 1, 2, 5, 6, 8, 9};

    // ===== Pentatonic scales =====
    scaleTemplates[ScaleType::MajorPentatonic] = {0, 2, 4, 7, 9};
    scaleTemplates[ScaleType::MinorPentatonic] = {0, 3, 5, 7, 10};
    scaleTemplates[ScaleType::EgyptianPentatonic] = {0, 2, 5, 7, 10};     // Suspended Pentatonic
    scaleTemplates[ScaleType::BluesMinorPentatonic] = {0, 3, 5, 8, 10};   // Man Gong
    scaleTemplates[ScaleType::BluesMajorPentatonic] = {0, 2, 5, 7, 9};    // Ritusen / Scottish
    scaleTemplates[ScaleType::JapanesePentatonic] = {0, 1, 5, 7, 8};      // In scale
    scaleTemplates[ScaleType::ChinesePentatonic] = {0, 2, 4, 7, 9};       // Same as major pentatonic

    // ===== Blues scales =====
    scaleTemplates[ScaleType::Blues] = {0, 3, 5, 6, 7, 10};               // Minor Blues
    scaleTemplates[ScaleType::MajorBlues] = {0, 2, 3, 4, 7, 9};           // Major Blues

    // ===== Bebop scales =====
    scaleTemplates[ScaleType::BebopDominant] = {0, 2, 4, 5, 7, 9, 10, 11};
    scaleTemplates[ScaleType::BebopMajor] = {0, 2, 4, 5, 7, 8, 9, 11};
    scaleTemplates[ScaleType::BebopMinor] = {0, 2, 3, 5, 7, 8, 9, 10};
    scaleTemplates[ScaleType::BebopDorian] = {0, 2, 3, 4, 5, 7, 9, 10};

    // ===== Symmetric scales =====
    scaleTemplates[ScaleType::Chromatic] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    scaleTemplates[ScaleType::WholeTone] = {0, 2, 4, 6, 8, 10};
    scaleTemplates[ScaleType::Diminished] = {0, 2, 3, 5, 6, 8, 9, 11};    // Whole-Half Diminished
    scaleTemplates[ScaleType::DiminishedHalfWhole] = {0, 1, 3, 4, 6, 7, 9, 10}; // Half-Whole Diminished
    scaleTemplates[ScaleType::Augmented] = {0, 3, 4, 7, 8, 11};           // Hexatonic

    // ===== Ethnic / World scales =====
    scaleTemplates[ScaleType::HungarianMajor] = {0, 3, 4, 6, 7, 9, 10};
    scaleTemplates[ScaleType::NeapolitanMajor] = {0, 1, 3, 5, 7, 9, 11};
    scaleTemplates[ScaleType::NeapolitanMinor] = {0, 1, 3, 5, 7, 8, 11};
    scaleTemplates[ScaleType::Persian] = {0, 1, 4, 5, 6, 8, 11};
    scaleTemplates[ScaleType::Hirajoshi] = {0, 2, 3, 7, 8};               // Japanese
    scaleTemplates[ScaleType::Iwato] = {0, 1, 5, 6, 10};                  // Japanese
    scaleTemplates[ScaleType::Kumoi] = {0, 2, 3, 7, 9};                   // Japanese
    scaleTemplates[ScaleType::InSen] = {0, 1, 5, 7, 10};                  // Japanese
    scaleTemplates[ScaleType::Mongolian] = {0, 2, 4, 7, 9};
    scaleTemplates[ScaleType::Balinese] = {0, 1, 3, 7, 8};
    scaleTemplates[ScaleType::Pelog] = {0, 1, 3, 7, 10};
    scaleTemplates[ScaleType::Algerian] = {0, 2, 3, 6, 7, 8, 11};
    scaleTemplates[ScaleType::Spanish8Tone] = {0, 1, 3, 4, 5, 6, 8, 10};
    scaleTemplates[ScaleType::Flamenco] = {0, 1, 4, 5, 7, 8, 11};
    scaleTemplates[ScaleType::Jewish] = {0, 1, 4, 5, 7, 8, 10};           // Ahava Rabbah
    scaleTemplates[ScaleType::Gypsy] = {0, 2, 3, 6, 7, 8, 10};
    scaleTemplates[ScaleType::Romanian] = {0, 2, 3, 6, 7, 9, 10};
    scaleTemplates[ScaleType::Hawaiian] = {0, 2, 3, 5, 7, 9, 11};
    scaleTemplates[ScaleType::Ethiopian] = {0, 2, 4, 5, 7, 8, 11};
    scaleTemplates[ScaleType::Arabic] = {0, 2, 4, 5, 6, 8, 10};

    // ===== Jazz scales =====
    scaleTemplates[ScaleType::Enigmatic] = {0, 1, 4, 6, 8, 10, 11};
    scaleTemplates[ScaleType::LeadingWholeTone] = {0, 2, 4, 6, 8, 10, 11};
    scaleTemplates[ScaleType::SixToneSymmetric] = {0, 1, 4, 5, 8, 9};
    scaleTemplates[ScaleType::Prometheus] = {0, 2, 4, 6, 9, 10};
    scaleTemplates[ScaleType::PrometheusNeapolitan] = {0, 1, 4, 6, 9, 10};
    scaleTemplates[ScaleType::Tritone] = {0, 1, 4, 6, 7, 10};
    scaleTemplates[ScaleType::TwoSemitoneTritone] = {0, 1, 2, 6, 7, 8};

    // ===== Modal variations =====
    scaleTemplates[ScaleType::MajorLocrian] = {0, 2, 4, 5, 6, 8, 10};
    scaleTemplates[ScaleType::ArabicMaqam] = {0, 1, 4, 5, 7, 8, 11};
    scaleTemplates[ScaleType::Istrian] = {0, 1, 3, 4, 6, 7};
    scaleTemplates[ScaleType::UkrainianDorian] = {0, 2, 3, 6, 7, 9, 10};
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
    std::vector<MIDIEvent> events = midiFile.getNoteEventsInRange(startTime, endTime);
    if (events.empty()) {
        return result;
    }
    result.noteWeights = calculateWeightedHistogram(events, midiFile);
    result.primaryScale = findBestScale(result.noteWeights);
    result.alternativeScales = findAlternativeScales(result.noteWeights, result.primaryScale);
    result.chordProgression = detectChordProgressions(midiFile, result.primaryScale);
    if (detectKeyChangesEnabled && (endTime - startTime) > 8.0) {
        result.keyChanges = this->detectKeyChanges(midiFile);
    }
    result.noteDistribution = calculateNoteDistribution(events);
    result.totalNotes = events.size();
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
                if (weightByDuration) {
                    double duration = event.timestamp - noteOn.timestamp;
                    weight *= duration;
                }
                if (weightByVelocity) {
                    weight *= (noteOn.velocity / 127.0);
                }
                histogram[pitchClass] += weight;
                activeNotes.erase(event.note);
            }
        }
    }
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

    for (int root = 0; root < 12; ++root) {
        std::array<double, 12> rotatedHistogram;
        for (int i = 0; i < 12; ++i) {
            rotatedHistogram[i] = histogram[(i + root) % 12];
        }
        double majorCorr = correlate(rotatedHistogram, majorProfile);
        if (majorCorr > bestCorrelation) {
            bestCorrelation = majorCorr;
            bestScale.root = intToNoteName(root);
            bestScale.type = ScaleType::Ionian;
            bestScale.intervals = scaleTemplates[ScaleType::Ionian];
            bestScale.confidence = (majorCorr + 1.0) / 2.0;
        }
        double minorCorr = correlate(rotatedHistogram, minorProfile);
        if (minorCorr > bestCorrelation) {
            bestCorrelation = minorCorr;
            bestScale.root = intToNoteName(root);
            bestScale.type = ScaleType::Aeolian;
            bestScale.intervals = scaleTemplates[ScaleType::Aeolian];
            bestScale.confidence = (minorCorr + 1.0) / 2.0;
        }
    }

    if (bestScale.type != ScaleType::Unknown) {
        int rootPitch = static_cast<int>(bestScale.root);
        for (const auto& templatePair : scaleTemplates) {
            if (templatePair.first == ScaleType::Ionian ||
                templatePair.first == ScaleType::Aeolian) {
                continue;
            }
            double matchScore = 0.0;
            double totalWeight = 0.0;
            for (int interval : templatePair.second) {
                int pitchClass = (rootPitch + interval) % 12;
                matchScore += histogram[pitchClass];
                totalWeight += 1.0;
            }
            matchScore /= totalWeight;
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
    for (int root = 0; root < 12; ++root) {
        std::array<double, 12> rotatedHistogram;
        for (int i = 0; i < 12; ++i) {
            rotatedHistogram[i] = histogram[(i + root) % 12];
        }
        double majorCorr = correlate(rotatedHistogram, majorProfile);
        double minorCorr = correlate(rotatedHistogram, minorProfile);
        double majorConf = (majorCorr + 1.0) / 2.0;
        double minorConf = (minorCorr + 1.0) / 2.0;
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
    std::sort(alternatives.begin(), alternatives.end(),
             [](const Scale& a, const Scale& b) {
                 return a.confidence > b.confidence;
             });
    if (alternatives.size() > 3) {
        alternatives.resize(3);
    }
    return alternatives;
}

std::vector<std::pair<double, Scale>> ScaleDetector::detectKeyChanges(const MIDIFile& midiFile) {
    std::vector<std::pair<double, Scale>> keyChanges;
    double duration = midiFile.getDuration();
    double windowSize = 4.0;
    double hopSize = 2.0;
    Scale previousScale;
    previousScale.type = ScaleType::Unknown;
    for (double time = 0.0; time < duration; time += hopSize) {
        double endTime = std::min(time + windowSize, duration);
        HarmonicAnalysis analysis = analyzeRange(midiFile, time, endTime);
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
    double windowSize = 1.0;
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
    std::map<int, MIDIEvent> noteStates;
    for (const auto& event : events) {
        if (event.timestamp > windowEnd) break;
        if (event.type == EventType::NoteOn && event.velocity > 0) {
            noteStates[event.note] = event;
        } else if (event.type == EventType::NoteOff ||
                  (event.type == EventType::NoteOn && event.velocity == 0)) {
            noteStates.erase(event.note);
        }
        if (event.timestamp >= windowStart && event.timestamp <= windowEnd) {
            for (const auto& pair : noteStates) {
                int pitchClass = pair.first % 12;
                activeNotes[pitchClass] = 1;
            }
        }
    }
    std::vector<int> activePitches;
    for (int i = 0; i < 12; ++i) {
        if (activeNotes[i] > 0) {
            activePitches.push_back(i);
        }
    }
    if (activePitches.size() < 2) {
        return "";
    }
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
        // Major modes
        case ScaleType::Ionian: return "Major";
        case ScaleType::Dorian: return "Dorian";
        case ScaleType::Phrygian: return "Phrygian";
        case ScaleType::Lydian: return "Lydian";
        case ScaleType::Mixolydian: return "Mixolydian";
        case ScaleType::Aeolian: return "Minor";
        case ScaleType::Locrian: return "Locrian";

        // Minor variants
        case ScaleType::HarmonicMinor: return "Harmonic Minor";
        case ScaleType::MelodicMinor: return "Melodic Minor";
        case ScaleType::NaturalMinor: return "Natural Minor";

        // Melodic Minor modes
        case ScaleType::DorianFlat2: return "Dorian b2";
        case ScaleType::LydianAugmented: return "Lydian Augmented";
        case ScaleType::LydianDominant: return "Lydian Dominant";
        case ScaleType::MixolydianFlat6: return "Mixolydian b6";
        case ScaleType::LocrianNatural2: return "Locrian #2";
        case ScaleType::SuperLocrian: return "Super Locrian";

        // Harmonic Minor modes
        case ScaleType::LocrianNatural6: return "Locrian #6";
        case ScaleType::IonianAugmented: return "Ionian Augmented";
        case ScaleType::DorianSharp4: return "Dorian #4";
        case ScaleType::PhrygianDominant: return "Phrygian Dominant";
        case ScaleType::LydianSharp2: return "Lydian #2";
        case ScaleType::SuperLocrianDiminished: return "Super Locrian Diminished";

        // Harmonic Major modes
        case ScaleType::HarmonicMajor: return "Harmonic Major";
        case ScaleType::DorianFlat5: return "Dorian b5";
        case ScaleType::PhrygianFlat4: return "Phrygian b4";
        case ScaleType::LydianFlat3: return "Lydian b3";
        case ScaleType::MixolydianFlat2: return "Mixolydian b2";
        case ScaleType::LydianAugmentedSharp2: return "Lydian Augmented #2";
        case ScaleType::LocrianDiminished7: return "Locrian Diminished 7";

        // Double Harmonic modes
        case ScaleType::DoubleHarmonic: return "Double Harmonic";
        case ScaleType::LydianSharp2Sharp6: return "Lydian #2 #6";
        case ScaleType::UltraPhrygian: return "Ultra Phrygian";
        case ScaleType::HungarianMinor: return "Hungarian Minor";
        case ScaleType::Oriental: return "Oriental";
        case ScaleType::IonianAugmentedSharp2: return "Ionian Augmented #2";
        case ScaleType::LocrianDiminished3Diminished7: return "Locrian bb3 bb7";

        // Pentatonic
        case ScaleType::MajorPentatonic: return "Major Pentatonic";
        case ScaleType::MinorPentatonic: return "Minor Pentatonic";
        case ScaleType::EgyptianPentatonic: return "Egyptian";
        case ScaleType::BluesMinorPentatonic: return "Blues Minor Pentatonic";
        case ScaleType::BluesMajorPentatonic: return "Blues Major Pentatonic";
        case ScaleType::JapanesePentatonic: return "Japanese";
        case ScaleType::ChinesePentatonic: return "Chinese";

        // Blues
        case ScaleType::Blues: return "Blues";
        case ScaleType::MajorBlues: return "Major Blues";

        // Bebop
        case ScaleType::BebopDominant: return "Bebop Dominant";
        case ScaleType::BebopMajor: return "Bebop Major";
        case ScaleType::BebopMinor: return "Bebop Minor";
        case ScaleType::BebopDorian: return "Bebop Dorian";

        // Symmetric
        case ScaleType::Chromatic: return "Chromatic";
        case ScaleType::WholeTone: return "Whole Tone";
        case ScaleType::Diminished: return "Diminished";
        case ScaleType::DiminishedHalfWhole: return "Diminished Half-Whole";
        case ScaleType::Augmented: return "Augmented";

        // Ethnic / World
        case ScaleType::HungarianMajor: return "Hungarian Major";
        case ScaleType::NeapolitanMajor: return "Neapolitan Major";
        case ScaleType::NeapolitanMinor: return "Neapolitan Minor";
        case ScaleType::Persian: return "Persian";
        case ScaleType::Hirajoshi: return "Hirajoshi";
        case ScaleType::Iwato: return "Iwato";
        case ScaleType::Kumoi: return "Kumoi";
        case ScaleType::InSen: return "In Sen";
        case ScaleType::Mongolian: return "Mongolian";
        case ScaleType::Balinese: return "Balinese";
        case ScaleType::Pelog: return "Pelog";
        case ScaleType::Algerian: return "Algerian";
        case ScaleType::Spanish8Tone: return "Spanish 8-Tone";
        case ScaleType::Flamenco: return "Flamenco";
        case ScaleType::Jewish: return "Jewish";
        case ScaleType::Gypsy: return "Gypsy";
        case ScaleType::Romanian: return "Romanian";
        case ScaleType::Hawaiian: return "Hawaiian";
        case ScaleType::Ethiopian: return "Ethiopian";
        case ScaleType::Arabic: return "Arabic";

        // Jazz
        case ScaleType::Enigmatic: return "Enigmatic";
        case ScaleType::LeadingWholeTone: return "Leading Whole Tone";
        case ScaleType::SixToneSymmetric: return "Six-Tone Symmetric";
        case ScaleType::Prometheus: return "Prometheus";
        case ScaleType::PrometheusNeapolitan: return "Prometheus Neapolitan";
        case ScaleType::Tritone: return "Tritone";
        case ScaleType::TwoSemitoneTritone: return "Two-Semitone Tritone";

        // Modal variations
        case ScaleType::MajorLocrian: return "Major Locrian";
        case ScaleType::ArabicMaqam: return "Arabic Maqam";
        case ScaleType::Istrian: return "Istrian";
        case ScaleType::UkrainianDorian: return "Ukrainian Dorian";

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
