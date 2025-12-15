import SwiftUI
import Combine
import SQLite3

@main
struct MIDIScaleDetectorApp: App {
    @StateObject private var appState = AppState()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(appState)
                .frame(minWidth: 1000, minHeight: 700)
        }
        .commands {
            MenuCommands(appState: appState)
        }

        Settings {
            SettingsView()
                .environmentObject(appState)
        }
    }
}

// MARK: - SQLite Database Manager
class DatabaseManager {
    private var db: OpaquePointer?
    let databasePath: String
    
    init(path: String) {
        self.databasePath = path
        openDatabase()
        createTables()
    }
    
    deinit {
        sqlite3_close(db)
    }
    
    private func openDatabase() {
        if sqlite3_open(databasePath, &db) != SQLITE_OK {
            print("Error opening database: \(String(cString: sqlite3_errmsg(db)))")
        }
    }
    
    private func createTables() {
        let createSQL = """
        CREATE TABLE IF NOT EXISTS midi_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT UNIQUE NOT NULL,
            file_name TEXT NOT NULL,
            detected_key TEXT,
            detected_scale TEXT,
            confidence REAL,
            tempo REAL,
            duration REAL,
            total_notes INTEGER,
            last_modified REAL,
            scan_date REAL,
            is_favorite INTEGER DEFAULT 0
        );
        
        CREATE TABLE IF NOT EXISTS scan_folders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            folder_path TEXT UNIQUE NOT NULL,
            last_scan REAL
        );
        
        CREATE INDEX IF NOT EXISTS idx_key ON midi_files(detected_key);
        CREATE INDEX IF NOT EXISTS idx_scale ON midi_files(detected_scale);
        """
        
        var errMsg: UnsafeMutablePointer<CChar>?
        if sqlite3_exec(db, createSQL, nil, nil, &errMsg) != SQLITE_OK {
            if let errMsg = errMsg {
                print("Error creating tables: \(String(cString: errMsg))")
                sqlite3_free(errMsg)
            }
        }
    }
    
    func addOrUpdateFile(_ file: MIDIFileModel) {
        let sql = """
        INSERT OR REPLACE INTO midi_files 
        (file_path, file_name, detected_key, detected_scale, confidence, tempo, duration, total_notes, last_modified, scan_date)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        """
        
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            sqlite3_bind_text(stmt, 1, file.filePath, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
            sqlite3_bind_text(stmt, 2, file.fileName, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
            sqlite3_bind_text(stmt, 3, file.detectedKey, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
            sqlite3_bind_text(stmt, 4, file.detectedScale, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
            sqlite3_bind_double(stmt, 5, file.confidence)
            sqlite3_bind_double(stmt, 6, file.tempo)
            sqlite3_bind_double(stmt, 7, file.duration)
            sqlite3_bind_int(stmt, 8, Int32(file.totalNotes))
            sqlite3_bind_double(stmt, 9, file.lastModified)
            sqlite3_bind_double(stmt, 10, Date().timeIntervalSince1970)
            
            sqlite3_step(stmt)
        }
        sqlite3_finalize(stmt)
    }
    
    func getAllFiles() -> [MIDIFileModel] {
        var files: [MIDIFileModel] = []
        let sql = "SELECT id, file_path, file_name, detected_key, detected_scale, confidence, tempo, duration, total_notes, last_modified, is_favorite FROM midi_files ORDER BY file_name;"
        
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            while sqlite3_step(stmt) == SQLITE_ROW {
                let file = MIDIFileModel(
                    id: Int(sqlite3_column_int(stmt, 0)),
                    filePath: String(cString: sqlite3_column_text(stmt, 1)),
                    fileName: String(cString: sqlite3_column_text(stmt, 2)),
                    detectedKey: String(cString: sqlite3_column_text(stmt, 3) ?? UnsafePointer<UInt8>(strdup("Unknown")!)),
                    detectedScale: String(cString: sqlite3_column_text(stmt, 4) ?? UnsafePointer<UInt8>(strdup("Unknown")!)),
                    confidence: sqlite3_column_double(stmt, 5),
                    tempo: sqlite3_column_double(stmt, 6),
                    duration: sqlite3_column_double(stmt, 7),
                    totalNotes: Int(sqlite3_column_int(stmt, 8)),
                    lastModified: sqlite3_column_double(stmt, 9),
                    isFavorite: sqlite3_column_int(stmt, 10) == 1
                )
                files.append(file)
            }
        }
        sqlite3_finalize(stmt)
        return files
    }
    
    func searchFiles(key: String?, scale: String?, searchText: String?) -> [MIDIFileModel] {
        var conditions: [String] = []
        
        if let key = key, key != "All" {
            conditions.append("detected_key = '\(key)'")
        }
        if let scale = scale, scale != "All" {
            conditions.append("detected_scale LIKE '%\(scale)%'")
        }
        if let text = searchText, !text.isEmpty {
            conditions.append("(file_name LIKE '%\(text)%' OR file_path LIKE '%\(text)%')")
        }
        
        var sql = "SELECT id, file_path, file_name, detected_key, detected_scale, confidence, tempo, duration, total_notes, last_modified, is_favorite FROM midi_files"
        if !conditions.isEmpty {
            sql += " WHERE " + conditions.joined(separator: " AND ")
        }
        sql += " ORDER BY file_name;"
        
        var files: [MIDIFileModel] = []
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            while sqlite3_step(stmt) == SQLITE_ROW {
                let file = MIDIFileModel(
                    id: Int(sqlite3_column_int(stmt, 0)),
                    filePath: String(cString: sqlite3_column_text(stmt, 1)),
                    fileName: String(cString: sqlite3_column_text(stmt, 2)),
                    detectedKey: String(cString: sqlite3_column_text(stmt, 3) ?? UnsafePointer<UInt8>(strdup("Unknown")!)),
                    detectedScale: String(cString: sqlite3_column_text(stmt, 4) ?? UnsafePointer<UInt8>(strdup("Unknown")!)),
                    confidence: sqlite3_column_double(stmt, 5),
                    tempo: sqlite3_column_double(stmt, 6),
                    duration: sqlite3_column_double(stmt, 7),
                    totalNotes: Int(sqlite3_column_int(stmt, 8)),
                    lastModified: sqlite3_column_double(stmt, 9),
                    isFavorite: sqlite3_column_int(stmt, 10) == 1
                )
                files.append(file)
            }
        }
        sqlite3_finalize(stmt)
        return files
    }
    
    func toggleFavorite(id: Int) {
        let sql = "UPDATE midi_files SET is_favorite = NOT is_favorite WHERE id = ?;"
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            sqlite3_bind_int(stmt, 1, Int32(id))
            sqlite3_step(stmt)
        }
        sqlite3_finalize(stmt)
    }
    
    func deleteFile(id: Int) {
        let sql = "DELETE FROM midi_files WHERE id = ?;"
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            sqlite3_bind_int(stmt, 1, Int32(id))
            sqlite3_step(stmt)
        }
        sqlite3_finalize(stmt)
    }
    
    func addScanFolder(_ path: String) {
        let sql = "INSERT OR REPLACE INTO scan_folders (folder_path, last_scan) VALUES (?, ?);"
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            sqlite3_bind_text(stmt, 1, path, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
            sqlite3_bind_double(stmt, 2, Date().timeIntervalSince1970)
            sqlite3_step(stmt)
        }
        sqlite3_finalize(stmt)
    }
    
    func getScanFolders() -> [String] {
        var folders: [String] = []
        let sql = "SELECT folder_path FROM scan_folders;"
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            while sqlite3_step(stmt) == SQLITE_ROW {
                folders.append(String(cString: sqlite3_column_text(stmt, 0)))
            }
        }
        sqlite3_finalize(stmt)
        return folders
    }
}

// MARK: - MIDI File Scanner
class MIDIScanner {
    
    // Scan a folder for MIDI files
    static func scanFolder(url: URL, recursive: Bool = true) -> [URL] {
        var midiFiles: [URL] = []
        let fm = FileManager.default
        
        guard let enumerator = fm.enumerator(
            at: url,
            includingPropertiesForKeys: [.isRegularFileKey, .contentModificationDateKey],
            options: recursive ? [] : [.skipsSubdirectoryDescendants]
        ) else {
            return midiFiles
        }
        
        for case let fileURL as URL in enumerator {
            let ext = fileURL.pathExtension.lowercased()
            if ext == "mid" || ext == "midi" || ext == "smf" {
                midiFiles.append(fileURL)
            }
        }
        
        return midiFiles
    }
    
    // Analyze a MIDI file and detect its scale
    static func analyzeMIDIFile(url: URL) -> MIDIFileModel? {
        guard let data = try? Data(contentsOf: url) else { return nil }
        
        // Parse MIDI header
        guard data.count > 14 else { return nil }
        
        // Get file attributes
        let attrs = try? FileManager.default.attributesOfItem(atPath: url.path)
        let modDate = (attrs?[.modificationDate] as? Date)?.timeIntervalSince1970 ?? 0
        
        // Parse MIDI to extract notes and tempo
        let (notes, tempo, duration) = parseMIDI(data: data)
        
        // Detect key and scale using note histogram
        let (key, scale, confidence) = detectScale(notes: notes)
        
        return MIDIFileModel(
            id: 0,
            filePath: url.path,
            fileName: url.lastPathComponent,
            detectedKey: key,
            detectedScale: scale,
            confidence: confidence,
            tempo: tempo,
            duration: duration,
            totalNotes: notes.count,
            lastModified: modDate,
            isFavorite: false
        )
    }
    
    private static func parseMIDI(data: Data) -> (notes: [Int], tempo: Double, duration: Double) {
        var notes: [Int] = []
        var tempo: Double = 120.0
        var ticksPerBeat: Int = 480
        var totalTicks: Int = 0
        
        // Check MIDI header "MThd"
        guard data.count > 14,
              data[0] == 0x4D, data[1] == 0x54,
              data[2] == 0x68, data[3] == 0x64 else {
            return (notes, tempo, 0)
        }
        
        // Read division (ticks per beat)
        ticksPerBeat = Int(data[12]) << 8 | Int(data[13])
        if ticksPerBeat >= 0x8000 {
            ticksPerBeat = 480 // SMPTE timing, use default
        }
        
        // Scan for tracks
        var index = 8 + 6 // Skip header
        while index < data.count - 8 {
            // Look for "MTrk"
            if data[index] == 0x4D && data[index + 1] == 0x54 &&
               data[index + 2] == 0x72 && data[index + 3] == 0x6B {
                
                let trackLength = Int(data[index + 4]) << 24 | Int(data[index + 5]) << 16 |
                                  Int(data[index + 6]) << 8 | Int(data[index + 7])
                
                let trackStart = index + 8
                let trackEnd = min(trackStart + trackLength, data.count)
                
                var pos = trackStart
                var runningStatus: UInt8 = 0
                
                while pos < trackEnd {
                    // Skip delta time (variable length)
                    while pos < trackEnd && data[pos] >= 0x80 {
                        if data[pos] < 0x80 { break }
                        pos += 1
                    }
                    if pos < trackEnd { pos += 1 }
                    
                    guard pos < trackEnd else { break }
                    
                    var status = data[pos]
                    
                    // Check for running status
                    if status < 0x80 {
                        status = runningStatus
                    } else {
                        pos += 1
                        if status < 0xF0 {
                            runningStatus = status
                        }
                    }
                    
                    let command = status & 0xF0
                    
                    switch command {
                    case 0x90: // Note On
                        if pos < trackEnd {
                            let note = Int(data[pos])
                            let velocity = pos + 1 < trackEnd ? data[pos + 1] : 0
                            if velocity > 0 {
                                notes.append(note % 12) // Store pitch class
                                totalTicks += 1
                            }
                            pos += 2
                        }
                    case 0x80: // Note Off
                        pos += 2
                    case 0xA0, 0xB0, 0xE0: // Aftertouch, Control, Pitch
                        pos += 2
                    case 0xC0, 0xD0: // Program, Channel Pressure
                        pos += 1
                    case 0xFF: // Meta event
                        if pos < trackEnd {
                            let metaType = data[pos]
                            pos += 1
                            
                            // Read length
                            var length = 0
                            while pos < trackEnd && data[pos] >= 0x80 {
                                length = (length << 7) | Int(data[pos] & 0x7F)
                                pos += 1
                            }
                            if pos < trackEnd {
                                length = (length << 7) | Int(data[pos] & 0x7F)
                                pos += 1
                            }
                            
                            // Tempo meta event
                            if metaType == 0x51 && length == 3 && pos + 2 < trackEnd {
                                let microsecondsPerBeat = Int(data[pos]) << 16 | Int(data[pos + 1]) << 8 | Int(data[pos + 2])
                                tempo = 60_000_000.0 / Double(microsecondsPerBeat)
                            }
                            
                            pos += length
                        }
                    case 0xF0, 0xF7: // SysEx
                        while pos < trackEnd && data[pos] != 0xF7 {
                            pos += 1
                        }
                        pos += 1
                    default:
                        pos += 1
                    }
                }
                
                index = trackEnd
            } else {
                index += 1
            }
        }
        
        // Calculate duration
        let duration = Double(totalTicks) / Double(ticksPerBeat) * (60.0 / tempo)
        
        return (notes, tempo, duration)
    }
    
    private static func detectScale(notes: [Int]) -> (key: String, scale: String, confidence: Double) {
        guard !notes.isEmpty else {
            return ("C", "Major", 0.0)
        }
        
        // Build histogram of pitch classes
        var histogram = [Double](repeating: 0, count: 12)
        for note in notes {
            histogram[note] += 1
        }
        
        // Normalize
        let total = histogram.reduce(0, +)
        if total > 0 {
            histogram = histogram.map { $0 / total }
        }
        
        // Krumhansl-Schmuckler key profiles
        let majorProfile: [Double] = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]
        let minorProfile: [Double] = [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]
        
        let noteNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
        
        var bestKey = "C"
        var bestScale = "Major"
        var bestCorrelation = -1.0
        
        // Test all keys
        for root in 0..<12 {
            // Rotate histogram
            var rotated = [Double](repeating: 0, count: 12)
            for i in 0..<12 {
                rotated[i] = histogram[(i + root) % 12]
            }
            
            // Correlate with major
            let majorCorr = correlate(rotated, majorProfile)
            if majorCorr > bestCorrelation {
                bestCorrelation = majorCorr
                bestKey = noteNames[root]
                bestScale = "Major"
            }
            
            // Correlate with minor
            let minorCorr = correlate(rotated, minorProfile)
            if minorCorr > bestCorrelation {
                bestCorrelation = minorCorr
                bestKey = noteNames[root]
                bestScale = "Minor"
            }
        }
        
        // Normalize confidence to 0-1
        let confidence = min(max((bestCorrelation + 1) / 2, 0), 1)
        
        return (bestKey, bestScale, confidence)
    }
    
    private static func correlate(_ a: [Double], _ b: [Double]) -> Double {
        guard a.count == b.count, !a.isEmpty else { return 0 }
        
        let n = Double(a.count)
        let sumA = a.reduce(0, +)
        let sumB = b.reduce(0, +)
        let meanA = sumA / n
        let meanB = sumB / n
        
        var num = 0.0
        var denA = 0.0
        var denB = 0.0
        
        for i in 0..<a.count {
            let diffA = a[i] - meanA
            let diffB = b[i] - meanB
            num += diffA * diffB
            denA += diffA * diffA
            denB += diffB * diffB
        }
        
        guard denA > 0 && denB > 0 else { return 0 }
        return num / sqrt(denA * denB)
    }
}

// MARK: - App State
class AppState: ObservableObject {
    @Published var midiFiles: [MIDIFileModel] = []
    @Published var selectedFile: MIDIFileModel?
    @Published var isScanning: Bool = false
    @Published var scanProgress: Double = 0.0
    @Published var scanStatus: String = ""
    @Published var filterKey: String = "All"
    @Published var filterScale: String = "All"
    @Published var searchText: String = ""
    @Published var savedFolders: [String] = []
    
    let database: DatabaseManager
    
    init() {
        let appSupport = FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        ).first!
        
        let appFolder = appSupport.appendingPathComponent("MIDIScaleDetector")
        try? FileManager.default.createDirectory(at: appFolder, withIntermediateDirectories: true)
        
        let dbPath = appFolder.appendingPathComponent("midi_library.db").path
        database = DatabaseManager(path: dbPath)
        
        loadFiles()
        loadSavedFolders()
    }
    
    func loadFiles() {
        midiFiles = database.getAllFiles()
    }
    
    func loadSavedFolders() {
        savedFolders = database.getScanFolders()
    }
    
    func startScan(paths: [URL]) {
        isScanning = true
        scanProgress = 0.0
        scanStatus = "Discovering MIDI files..."
        
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self = self else { return }
            
            var allMIDIFiles: [URL] = []
            
            // Discover all MIDI files
            for path in paths {
                self.database.addScanFolder(path.path)
                let files = MIDIScanner.scanFolder(url: path, recursive: true)
                allMIDIFiles.append(contentsOf: files)
            }
            
            let total = allMIDIFiles.count
            
            DispatchQueue.main.async {
                self.scanStatus = "Found \(total) MIDI files. Analyzing..."
            }
            
            // Analyze each file
            for (index, fileURL) in allMIDIFiles.enumerated() {
                if let analyzed = MIDIScanner.analyzeMIDIFile(url: fileURL) {
                    self.database.addOrUpdateFile(analyzed)
                }
                
                let progress = Double(index + 1) / Double(total)
                
                DispatchQueue.main.async {
                    self.scanProgress = progress
                    self.scanStatus = "Analyzing: \(fileURL.lastPathComponent)"
                }
            }
            
            DispatchQueue.main.async {
                self.isScanning = false
                self.scanStatus = ""
                self.loadFiles()
                self.loadSavedFolders()
            }
        }
    }
    
    func cancelScan() {
        isScanning = false
        scanStatus = ""
    }
    
    func filteredFiles() -> [MIDIFileModel] {
        if filterKey == "All" && filterScale == "All" && searchText.isEmpty {
            return midiFiles
        }
        
        return midiFiles.filter { file in
            let keyMatch = filterKey == "All" || file.detectedKey == filterKey
            let scaleMatch = filterScale == "All" || file.detectedScale.contains(filterScale)
            let searchMatch = searchText.isEmpty ||
                file.fileName.localizedCaseInsensitiveContains(searchText) ||
                file.filePath.localizedCaseInsensitiveContains(searchText)
            
            return keyMatch && scaleMatch && searchMatch
        }
    }
    
    func toggleFavorite(file: MIDIFileModel) {
        database.toggleFavorite(id: file.id)
        loadFiles()
    }
    
    func deleteFile(file: MIDIFileModel) {
        database.deleteFile(id: file.id)
        if selectedFile?.id == file.id {
            selectedFile = nil
        }
        loadFiles()
    }
    
    func rescanAllFolders() {
        let urls = savedFolders.map { URL(fileURLWithPath: $0) }
        startScan(paths: urls)
    }
}

// MARK: - MIDI File Model
struct MIDIFileModel: Identifiable, Hashable {
    let id: Int
    let filePath: String
    let fileName: String
    let detectedKey: String
    let detectedScale: String
    let confidence: Double
    let tempo: Double
    let duration: Double
    let totalNotes: Int
    var lastModified: Double = 0
    var isFavorite: Bool = false
    
    func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
    
    static func == (lhs: MIDIFileModel, rhs: MIDIFileModel) -> Bool {
        lhs.id == rhs.id
    }
}
