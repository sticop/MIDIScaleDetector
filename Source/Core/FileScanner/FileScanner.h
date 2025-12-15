#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include "../Database/Database.h"
#include "../MIDIParser/MIDIParser.h"
#include "../ScaleDetector/ScaleDetector.h"

namespace MIDIScaleDetector {

// Scan progress callback
using ProgressCallback = std::function<void(int current, int total, const std::string& currentFile)>;

// File scanner configuration
struct ScannerConfig {
    std::vector<std::string> searchPaths;
    std::vector<std::string> excludePaths;
    bool recursive;
    bool rescanModified;
    int maxThreads;
    
    ScannerConfig() : recursive(true), rescanModified(true), maxThreads(4) {}
};

// Scanner statistics
struct ScanStats {
    int totalFiles;
    int newFiles;
    int updatedFiles;
    int failedFiles;
    double scanDuration;
    
    ScanStats() : totalFiles(0), newFiles(0), updatedFiles(0), 
                 failedFiles(0), scanDuration(0.0) {}
};

// File Scanner Class
class FileScanner {
public:
    FileScanner(Database& database);
    ~FileScanner();
    
    // Start scanning
    bool startScan(const ScannerConfig& config, ProgressCallback callback = nullptr);
    
    // Stop current scan
    void stopScan();
    
    // Check if scanning
    bool isScanning() const { return scanning.load(); }
    
    // Get last scan statistics
    ScanStats getLastScanStats() const { return lastStats; }
    
    // Quick scan single file
    bool scanFile(const std::string& filePath);
    
    // Rescan all files in database
    bool rescanAll(ProgressCallback callback = nullptr);
    
private:
    Database& db;
    MIDIParser parser;
    ScaleDetector detector;
    
    std::atomic<bool> scanning;
    std::atomic<bool> shouldStop;
    ScanStats lastStats;
    
    // Internal scan methods
    void scanDirectory(const std::string& path, bool recursive,
                      std::vector<std::string>& foundFiles);
    
    bool isMIDIFile(const std::string& filePath);
    bool isExcluded(const std::string& filePath, const std::vector<std::string>& excludePaths);
    
    int64_t getFileModifiedTime(const std::string& filePath);
    int64_t getFileSize(const std::string& filePath);
    
    bool analyzeAndStore(const std::string& filePath);
    
    MIDIFileEntry createEntry(const std::string& filePath, 
                             const MIDIFile& midiFile,
                             const HarmonicAnalysis& analysis);
};

} // namespace MIDIScaleDetector
