#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>
#include "../ScaleDetector/ScaleDetector.h"

namespace MIDIScaleDetector {

// Database entry for a MIDI file
struct MIDIFileEntry {
    int id;
    std::string filePath;
    std::string fileName;
    int64_t fileSize;
    int64_t lastModified;
    
    // Musical properties
    std::string detectedKey;
    std::string detectedScale;
    double confidence;
    double tempo;
    double duration;
    
    // Additional metadata
    int totalNotes;
    double averagePitch;
    std::string chordProgression;
    
    // Timestamps
    int64_t dateAdded;
    int64_t dateAnalyzed;
    
    MIDIFileEntry() : id(-1), fileSize(0), lastModified(0), confidence(0.0),
                     tempo(120.0), duration(0.0), totalNotes(0), 
                     averagePitch(0.0), dateAdded(0), dateAnalyzed(0) {}
};

// Search/filter criteria
struct SearchCriteria {
    std::string keyFilter;           // e.g., "C", "D", etc.
    std::string scaleFilter;         // e.g., "Major", "Minor", etc.
    double minConfidence;
    double maxConfidence;
    double minTempo;
    double maxTempo;
    double minDuration;
    double maxDuration;
    std::string pathFilter;          // Search in specific directory
    
    SearchCriteria() : minConfidence(0.0), maxConfidence(1.0),
                      minTempo(0.0), maxTempo(999.0),
                      minDuration(0.0), maxDuration(9999.0) {}
};

// Database manager class
class Database {
public:
    Database();
    ~Database();
    
    // Initialize database connection
    bool initialize(const std::string& dbPath);
    
    // Close database
    void close();
    
    // File operations
    bool addFile(const MIDIFileEntry& entry);
    bool updateFile(const MIDIFileEntry& entry);
    bool removeFile(const std::string& filePath);
    bool fileExists(const std::string& filePath);
    
    // Retrieval
    MIDIFileEntry getFile(int id);
    MIDIFileEntry getFile(const std::string& filePath);
    std::vector<MIDIFileEntry> getAllFiles();
    std::vector<MIDIFileEntry> search(const SearchCriteria& criteria);
    
    // Statistics
    int getTotalFileCount();
    std::vector<std::pair<std::string, int>> getKeyDistribution();
    std::vector<std::pair<std::string, int>> getScaleDistribution();
    
    // Maintenance
    bool vacuum();
    bool rebuildIndex();
    
    // Error handling
    std::string getLastError() const { return lastError; }
    
private:
    sqlite3* db;
    std::string lastError;
    
    // Internal helpers
    bool createTables();
    bool executeSQL(const std::string& sql);
    MIDIFileEntry parseRow(sqlite3_stmt* stmt);
    std::string buildSearchQuery(const SearchCriteria& criteria);
};

} // namespace MIDIScaleDetector
