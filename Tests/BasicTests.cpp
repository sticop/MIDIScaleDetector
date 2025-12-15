#include <cassert>
#include <iostream>
#include "../Source/Core/MIDIParser/MIDIParser.h"
#include "../Source/Core/ScaleDetector/ScaleDetector.h"

using namespace MIDIScaleDetector;

void testMIDIParser() {
    std::cout << "Testing MIDI Parser..." << std::endl;
    
    MIDIParser parser;
    
    // Test header parsing with sample data
    // Note: This would require actual MIDI test files
    // For now, we verify the parser exists and can be instantiated
    
    std::cout << "  ✓ MIDI Parser initialized" << std::endl;
}

void testScaleDetector() {
    std::cout << "Testing Scale Detector..." << std::endl;
    
    ScaleDetector detector;
    
    // Test scale templates initialization
    detector.setMinConfidenceThreshold(0.7);
    
    std::cout << "  ✓ Scale Detector initialized" << std::endl;
    std::cout << "  ✓ Confidence threshold set" << std::endl;
}

void testScaleTypes() {
    std::cout << "Testing Scale Type Conversions..." << std::endl;
    
    // Test note name conversion
    assert(noteNameToString(NoteName::C) == "C");
    assert(noteNameToString(NoteName::Db) == "Db");
    assert(noteNameToString(NoteName::G) == "G");
    
    // Test scale type conversion
    assert(scaleTypeToString(ScaleType::Ionian) == "Major");
    assert(scaleTypeToString(ScaleType::Aeolian) == "Minor");
    assert(scaleTypeToString(ScaleType::Dorian) == "Dorian");
    
    std::cout << "  ✓ Note name conversions correct" << std::endl;
    std::cout << "  ✓ Scale type conversions correct" << std::endl;
}

void testScale() {
    std::cout << "Testing Scale Structure..." << std::endl;
    
    Scale scale;
    scale.root = NoteName::C;
    scale.type = ScaleType::Ionian;
    scale.intervals = {0, 2, 4, 5, 7, 9, 11}; // C Major scale
    scale.confidence = 0.95;
    
    // Test scale name
    std::string name = scale.getName();
    assert(name == "C Major");
    
    // Test note containment
    assert(scale.containsNote(60) == true);  // C (middle C)
    assert(scale.containsNote(62) == true);  // D
    assert(scale.containsNote(64) == true);  // E
    assert(scale.containsNote(61) == false); // C# (not in C Major)
    
    std::cout << "  ✓ Scale name: " << name << std::endl;
    std::cout << "  ✓ Note containment checks correct" << std::endl;
}

void testDatabase() {
    std::cout << "Testing Database..." << std::endl;
    
    Database db;
    
    // Test database initialization
    bool initialized = db.initialize(":memory:"); // Use in-memory database for testing
    assert(initialized);
    
    std::cout << "  ✓ Database initialized" << std::endl;
    
    // Test file entry
    MIDIFileEntry entry;
    entry.filePath = "/test/file.mid";
    entry.fileName = "file.mid";
    entry.detectedKey = "C";
    entry.detectedScale = "Major";
    entry.confidence = 0.85;
    entry.tempo = 120.0;
    entry.duration = 180.0;
    entry.totalNotes = 500;
    entry.dateAdded = 1234567890;
    entry.dateAnalyzed = 1234567890;
    
    bool added = db.addFile(entry);
    assert(added);
    
    std::cout << "  ✓ File entry added" << std::endl;
    
    // Test retrieval
    bool exists = db.fileExists("/test/file.mid");
    assert(exists);
    
    std::cout << "  ✓ File retrieval works" << std::endl;
    
    // Test search
    SearchCriteria criteria;
    criteria.keyFilter = "C";
    auto results = db.search(criteria);
    assert(results.size() > 0);
    
    std::cout << "  ✓ Search functionality works" << std::endl;
    
    db.close();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "MIDI Scale Detector - Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        testMIDIParser();
        std::cout << std::endl;
        
        testScaleDetector();
        std::cout << std::endl;
        
        testScaleTypes();
        std::cout << std::endl;
        
        testScale();
        std::cout << std::endl;
        
        testDatabase();
        std::cout << std::endl;
        
        std::cout << "========================================" << std::endl;
        std::cout << "All tests passed! ✓" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
