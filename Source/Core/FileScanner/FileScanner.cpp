#include "FileScanner.h"
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

namespace MIDIScaleDetector {

FileScanner::FileScanner(Database& database) 
    : db(database), scanning(false), shouldStop(false) {}

FileScanner::~FileScanner() {
    stopScan();
}

bool FileScanner::startScan(const ScannerConfig& config, ProgressCallback callback) {
    if (scanning.load()) {
        return false;
    }
    
    scanning = true;
    shouldStop = false;
    lastStats = ScanStats();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Find all MIDI files
    std::vector<std::string> allFiles;
    
    for (const auto& searchPath : config.searchPaths) {
        if (shouldStop.load()) break;
        
        scanDirectory(searchPath, config.recursive, allFiles);
    }
    
    // Filter out excluded paths
    std::vector<std::string> filesToScan;
    for (const auto& file : allFiles) {
        if (!isExcluded(file, config.excludePaths)) {
            filesToScan.push_back(file);
        }
    }
    
    lastStats.totalFiles = filesToScan.size();
    
    // Process files
    int processed = 0;
    for (const auto& filePath : filesToScan) {
        if (shouldStop.load()) break;
        
        if (callback) {
            callback(processed, lastStats.totalFiles, filePath);
        }
        
        // Check if file needs scanning
        bool needsScan = true;
        
        if (db.fileExists(filePath) && !config.rescanModified) {
            needsScan = false;
        } else if (db.fileExists(filePath) && config.rescanModified) {
            // Check if modified
            MIDIFileEntry existing = db.getFile(filePath);
            int64_t currentModTime = getFileModifiedTime(filePath);
            
            if (currentModTime <= existing.lastModified) {
                needsScan = false;
            }
        }
        
        if (needsScan) {
            if (analyzeAndStore(filePath)) {
                if (db.fileExists(filePath)) {
                    lastStats.updatedFiles++;
                } else {
                    lastStats.newFiles++;
                }
            } else {
                lastStats.failedFiles++;
            }
        }
        
        processed++;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    lastStats.scanDuration = elapsed.count();
    
    scanning = false;
    
    return true;
}

void FileScanner::stopScan() {
    shouldStop = true;
    
    // Wait for scanning to complete
    while (scanning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool FileScanner::scanFile(const std::string& filePath) {
    if (!isMIDIFile(filePath)) {
        return false;
    }
    
    return analyzeAndStore(filePath);
}

bool FileScanner::rescanAll(ProgressCallback callback) {
    auto allFiles = db.getAllFiles();
    
    int processed = 0;
    int total = allFiles.size();
    
    for (const auto& entry : allFiles) {
        if (callback) {
            callback(processed++, total, entry.filePath);
        }
        
        // Check if file still exists
        if (!fs::exists(entry.filePath)) {
            db.removeFile(entry.filePath);
            continue;
        }
        
        // Rescan
        analyzeAndStore(entry.filePath);
    }
    
    return true;
}

void FileScanner::scanDirectory(const std::string& path, bool recursive,
                               std::vector<std::string>& foundFiles) {
    
    if (!fs::exists(path) || !fs::is_directory(path)) {
        return;
    }
    
    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (shouldStop.load()) break;
                
                if (entry.is_regular_file() && isMIDIFile(entry.path().string())) {
                    foundFiles.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (shouldStop.load()) break;
                
                if (entry.is_regular_file() && isMIDIFile(entry.path().string())) {
                    foundFiles.push_back(entry.path().string());
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        // Handle permission errors, etc.
    }
}

bool FileScanner::isMIDIFile(const std::string& filePath) {
    fs::path path(filePath);
    std::string ext = path.extension().string();
    
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return ext == ".mid" || ext == ".midi";
}

bool FileScanner::isExcluded(const std::string& filePath, 
                            const std::vector<std::string>& excludePaths) {
    
    for (const auto& excludePath : excludePaths) {
        if (filePath.find(excludePath) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

int64_t FileScanner::getFileModifiedTime(const std::string& filePath) {
    try {
        auto ftime = fs::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        return std::chrono::system_clock::to_time_t(sctp);
    } catch (...) {
        return 0;
    }
}

int64_t FileScanner::getFileSize(const std::string& filePath) {
    try {
        return fs::file_size(filePath);
    } catch (...) {
        return 0;
    }
}

bool FileScanner::analyzeAndStore(const std::string& filePath) {
    // Parse MIDI file
    MIDIFile midiFile;
    if (!parser.parse(filePath, midiFile)) {
        return false;
    }
    
    // Analyze scale
    HarmonicAnalysis analysis = detector.analyze(midiFile);
    
    // Create database entry
    MIDIFileEntry entry = createEntry(filePath, midiFile, analysis);
    
    // Store or update
    if (db.fileExists(filePath)) {
        return db.updateFile(entry);
    } else {
        return db.addFile(entry);
    }
}

MIDIFileEntry FileScanner::createEntry(const std::string& filePath,
                                      const MIDIFile& midiFile,
                                      const HarmonicAnalysis& analysis) {
    MIDIFileEntry entry;
    
    fs::path path(filePath);
    
    entry.filePath = filePath;
    entry.fileName = path.filename().string();
    entry.fileSize = getFileSize(filePath);
    entry.lastModified = getFileModifiedTime(filePath);
    
    // Musical properties
    entry.detectedKey = analysis.primaryScale.getRootName();
    entry.detectedScale = scaleTypeToString(analysis.primaryScale.type);
    entry.confidence = analysis.primaryScale.confidence;
    entry.tempo = midiFile.tempo;
    entry.duration = midiFile.getDuration();
    
    // Additional metadata
    entry.totalNotes = analysis.totalNotes;
    entry.averagePitch = analysis.averagePitch;
    
    // Chord progression (join with commas)
    std::ostringstream oss;
    for (size_t i = 0; i < analysis.chordProgression.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << analysis.chordProgression[i];
    }
    entry.chordProgression = oss.str();
    
    // Timestamps
    auto now = std::chrono::system_clock::now();
    entry.dateAdded = std::chrono::system_clock::to_time_t(now);
    entry.dateAnalyzed = entry.dateAdded;
    
    return entry;
}

} // namespace MIDIScaleDetector
