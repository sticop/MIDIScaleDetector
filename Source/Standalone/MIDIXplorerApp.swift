import SwiftUI
import Combine
import SQLite3
import CoreMIDI
import AVFoundation
import UniformTypeIdentifiers

@main
struct MIDIXplorerApp: App {
    @StateObject private var appState = AppState()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(appState)
                .frame(minWidth: 1200, minHeight: 800)
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
            time_signature TEXT DEFAULT '4/4',
            last_modified REAL,
            scan_date REAL,
            is_favorite INTEGER DEFAULT 0,
            play_count INTEGER DEFAULT 0,
            rating INTEGER DEFAULT 0,
            tags TEXT DEFAULT ''
        );

        CREATE TABLE IF NOT EXISTS scan_folders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            folder_path TEXT UNIQUE NOT NULL,
            last_scan REAL
        );

        CREATE INDEX IF NOT EXISTS idx_key ON midi_files(detected_key);
        CREATE INDEX IF NOT EXISTS idx_scale ON midi_files(detected_scale);
        CREATE INDEX IF NOT EXISTS idx_tempo ON midi_files(tempo);
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
        (file_path, file_name, detected_key, detected_scale, confidence, tempo, duration, total_notes, time_signature, last_modified, scan_date)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
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
            sqlite3_bind_text(stmt, 9, file.timeSignature, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
            sqlite3_bind_double(stmt, 10, file.lastModified)
            sqlite3_bind_double(stmt, 11, Date().timeIntervalSince1970)

            sqlite3_step(stmt)
        }
        sqlite3_finalize(stmt)
    }

    func getAllFiles() -> [MIDIFileModel] {
        var files: [MIDIFileModel] = []
        let sql = "SELECT id, file_path, file_name, detected_key, detected_scale, confidence, tempo, duration, total_notes, time_signature, last_modified, is_favorite, play_count, rating, tags FROM midi_files ORDER BY file_name;"

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
                    timeSignature: String(cString: sqlite3_column_text(stmt, 9) ?? UnsafePointer<UInt8>(strdup("4/4")!)),
                    lastModified: sqlite3_column_double(stmt, 10),
                    isFavorite: sqlite3_column_int(stmt, 11) == 1,
                    playCount: Int(sqlite3_column_int(stmt, 12)),
                    rating: Int(sqlite3_column_int(stmt, 13)),
                    tags: String(cString: sqlite3_column_text(stmt, 14) ?? UnsafePointer<UInt8>(strdup(""))!)
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

    func incrementPlayCount(id: Int) {
        let sql = "UPDATE midi_files SET play_count = play_count + 1 WHERE id = ?;"
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            sqlite3_bind_int(stmt, 1, Int32(id))
            sqlite3_step(stmt)
        }
        sqlite3_finalize(stmt)
    }

    func setRating(id: Int, rating: Int) {
        let sql = "UPDATE midi_files SET rating = ? WHERE id = ?;"
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            sqlite3_bind_int(stmt, 1, Int32(rating))
            sqlite3_bind_int(stmt, 2, Int32(id))
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

    func removeScanFolder(_ path: String) {
        let sql = "DELETE FROM scan_folders WHERE folder_path = ?;"
        var stmt: OpaquePointer?
        if sqlite3_prepare_v2(db, sql, -1, &stmt, nil) == SQLITE_OK {
            sqlite3_bind_text(stmt, 1, path, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
            sqlite3_step(stmt)
        }
        sqlite3_finalize(stmt)
    }
}

// MARK: - MIDI Player with DAW Sync
class MIDIPlayer: ObservableObject {
    @Published var isPlaying = false
    @Published var currentPosition: Double = 0
    @Published var syncMode: SyncMode = .internal_
    @Published var tempo: Double = 120.0

    private var midiClient = MIDIClientRef()
    private var outputPort = MIDIPortRef()
    private var virtualSource = MIDIEndpointRef()
    private var currentEvents: [MIDIPlayEvent] = []
    private var playbackTimer: Timer?
    private var startTime: Date?

    enum SyncMode: String, CaseIterable {
        case internal_ = "Internal"
        case midiClock = "MIDI Clock"
        case abletonLink = "Ableton Link"
    }

    struct MIDIPlayEvent {
        let time: Double // in beats
        let status: UInt8
        let data1: UInt8
        let data2: UInt8
    }

    init() {
        setupMIDI()
    }

    private func setupMIDI() {
        // Create MIDI client
        MIDIClientCreate("MIDIXplorer" as CFString, nil, nil, &midiClient)

        // Create output port
        MIDIOutputPortCreate(midiClient, "Output" as CFString, &outputPort)

        // Create virtual source for sending MIDI to DAWs
        MIDISourceCreate(midiClient, "MIDI Xplorer" as CFString, &virtualSource)
    }

    func loadMIDIFile(url: URL) -> Bool {
        guard let data = try? Data(contentsOf: url) else { return false }
        currentEvents = parseMIDIToEvents(data: data)
        return true
    }

    private func parseMIDIToEvents(data: Data) -> [MIDIPlayEvent] {
        var events: [MIDIPlayEvent] = []
        var ticksPerBeat: Double = 480

        // Check MIDI header "MThd"
        guard data.count > 14,
              data[0] == 0x4D, data[1] == 0x54,
              data[2] == 0x68, data[3] == 0x64 else {
            return events
        }

        // Read division (ticks per beat)
        let division = Int(data[12]) << 8 | Int(data[13])
        if division < 0x8000 {
            ticksPerBeat = Double(division)
        }

        // Parse tracks
        var index = 14
        while index < data.count - 8 {
            // Look for "MTrk"
            if data[index] == 0x4D && data[index + 1] == 0x54 &&
               data[index + 2] == 0x72 && data[index + 3] == 0x6B {

                let trackLength = Int(data[index + 4]) << 24 | Int(data[index + 5]) << 16 |
                                  Int(data[index + 6]) << 8 | Int(data[index + 7])

                let trackStart = index + 8
                let trackEnd = min(trackStart + trackLength, data.count)

                var pos = trackStart
                var absoluteTick: Int = 0
                var runningStatus: UInt8 = 0

                while pos < trackEnd {
                    // Read delta time (variable length)
                    var deltaTime = 0
                    while pos < trackEnd {
                        let byte = data[pos]
                        deltaTime = (deltaTime << 7) | Int(byte & 0x7F)
                        pos += 1
                        if byte < 0x80 { break }
                    }

                    absoluteTick += deltaTime
                    let beatPosition = Double(absoluteTick) / ticksPerBeat

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
                    case 0x90, 0x80: // Note On/Off
                        if pos + 1 < trackEnd {
                            let note = data[pos]
                            let velocity = data[pos + 1]
                            events.append(MIDIPlayEvent(time: beatPosition, status: status, data1: note, data2: velocity))
                            pos += 2
                        }
                    case 0xA0, 0xB0, 0xE0:
                        pos += 2
                    case 0xC0, 0xD0:
                        pos += 1
                    case 0xFF: // Meta event
                        if pos < trackEnd {
                            pos += 1 // meta type
                            var length = 0
                            while pos < trackEnd {
                                let byte = data[pos]
                                length = (length << 7) | Int(byte & 0x7F)
                                pos += 1
                                if byte < 0x80 { break }
                            }
                            pos += length
                        }
                    case 0xF0, 0xF7:
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

        return events.sorted { $0.time < $1.time }
    }

    func play(atTempo: Double? = nil) {
        if let t = atTempo {
            tempo = t
        }

        isPlaying = true
        startTime = Date()
        currentPosition = 0

        // Schedule playback
        let secondsPerBeat = 60.0 / tempo

        playbackTimer = Timer.scheduledTimer(withTimeInterval: 0.001, repeats: true) { [weak self] _ in
            guard let self = self, self.isPlaying else { return }

            let elapsed = Date().timeIntervalSince(self.startTime!)
            let currentBeat = elapsed / secondsPerBeat
            self.currentPosition = currentBeat

            // Send events that should play now
            for event in self.currentEvents {
                if event.time >= currentBeat - 0.01 && event.time < currentBeat + 0.01 {
                    self.sendMIDIEvent(event)
                }
            }

            // Check if playback is complete
            if let lastEvent = self.currentEvents.last, currentBeat > lastEvent.time + 1 {
                self.stop()
            }
        }
    }

    func stop() {
        isPlaying = false
        playbackTimer?.invalidate()
        playbackTimer = nil

        // Send all notes off
        for channel: UInt8 in 0..<16 {
            sendControlChange(channel: channel, controller: 123, value: 0) // All notes off
        }
    }

    private func sendMIDIEvent(_ event: MIDIPlayEvent) {
        var packet = MIDIPacket()
        packet.timeStamp = 0
        packet.length = 3
        packet.data.0 = event.status
        packet.data.1 = event.data1
        packet.data.2 = event.data2

        var packetList = MIDIPacketList(numPackets: 1, packet: packet)
        MIDIReceived(virtualSource, &packetList)
    }

    private func sendControlChange(channel: UInt8, controller: UInt8, value: UInt8) {
        var packet = MIDIPacket()
        packet.timeStamp = 0
        packet.length = 3
        packet.data.0 = 0xB0 | channel
        packet.data.1 = controller
        packet.data.2 = value

        var packetList = MIDIPacketList(numPackets: 1, packet: packet)
        MIDIReceived(virtualSource, &packetList)
    }
}

// MARK: - MIDI File Scanner
class MIDIScanner {

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

    static func analyzeMIDIFile(url: URL) -> MIDIFileModel? {
        guard let data = try? Data(contentsOf: url) else { return nil }
        guard data.count > 14 else { return nil }

        let attrs = try? FileManager.default.attributesOfItem(atPath: url.path)
        let modDate = (attrs?[.modificationDate] as? Date)?.timeIntervalSince1970 ?? 0

        let (notes, tempo, duration, timeSignature) = parseMIDI(data: data)
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
            timeSignature: timeSignature,
            lastModified: modDate,
            isFavorite: false,
            playCount: 0,
            rating: 0,
            tags: ""
        )
    }

    private static func parseMIDI(data: Data) -> (notes: [Int], tempo: Double, duration: Double, timeSignature: String) {
        var notes: [Int] = []
        var tempo: Double = 120.0
        var ticksPerBeat: Int = 480
        var totalTicks: Int = 0
        var timeSignature = "4/4"

        guard data.count > 14,
              data[0] == 0x4D, data[1] == 0x54,
              data[2] == 0x68, data[3] == 0x64 else {
            return (notes, tempo, 0, timeSignature)
        }

        ticksPerBeat = Int(data[12]) << 8 | Int(data[13])
        if ticksPerBeat >= 0x8000 {
            ticksPerBeat = 480
        }

        var index = 14
        while index < data.count - 8 {
            if data[index] == 0x4D && data[index + 1] == 0x54 &&
               data[index + 2] == 0x72 && data[index + 3] == 0x6B {

                let trackLength = Int(data[index + 4]) << 24 | Int(data[index + 5]) << 16 |
                                  Int(data[index + 6]) << 8 | Int(data[index + 7])

                let trackStart = index + 8
                let trackEnd = min(trackStart + trackLength, data.count)

                var pos = trackStart
                var runningStatus: UInt8 = 0

                while pos < trackEnd {
                    while pos < trackEnd && data[pos] >= 0x80 {
                        if data[pos] < 0x80 { break }
                        pos += 1
                    }
                    if pos < trackEnd { pos += 1 }

                    guard pos < trackEnd else { break }

                    var status = data[pos]

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
                    case 0x90:
                        if pos < trackEnd {
                            let note = Int(data[pos])
                            let velocity = pos + 1 < trackEnd ? data[pos + 1] : 0
                            if velocity > 0 {
                                notes.append(note % 12)
                                totalTicks += 1
                            }
                            pos += 2
                        }
                    case 0x80:
                        pos += 2
                    case 0xA0, 0xB0, 0xE0:
                        pos += 2
                    case 0xC0, 0xD0:
                        pos += 1
                    case 0xFF:
                        if pos < trackEnd {
                            let metaType = data[pos]
                            pos += 1

                            var length = 0
                            while pos < trackEnd && data[pos] >= 0x80 {
                                length = (length << 7) | Int(data[pos] & 0x7F)
                                pos += 1
                            }
                            if pos < trackEnd {
                                length = (length << 7) | Int(data[pos] & 0x7F)
                                pos += 1
                            }

                            if metaType == 0x51 && length == 3 && pos + 2 < trackEnd {
                                let microsecondsPerBeat = Int(data[pos]) << 16 | Int(data[pos + 1]) << 8 | Int(data[pos + 2])
                                tempo = 60_000_000.0 / Double(microsecondsPerBeat)
                            }

                            if metaType == 0x58 && length == 4 && pos + 3 < trackEnd {
                                let numerator = Int(data[pos])
                                let denominator = Int(pow(2.0, Double(data[pos + 1])))
                                timeSignature = "\(numerator)/\(denominator)"
                            }

                            pos += length
                        }
                    case 0xF0, 0xF7:
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

        let duration = Double(totalTicks) / Double(ticksPerBeat) * (60.0 / tempo)

        return (notes, tempo, duration, timeSignature)
    }

    private static func detectScale(notes: [Int]) -> (key: String, scale: String, confidence: Double) {
        guard !notes.isEmpty else {
            return ("C", "Major", 0.0)
        }

        var histogram = [Double](repeating: 0, count: 12)
        for note in notes {
            histogram[note] += 1
        }

        let total = histogram.reduce(0, +)
        if total > 0 {
            histogram = histogram.map { $0 / total }
        }

        let majorProfile: [Double] = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]
        let minorProfile: [Double] = [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]

        let noteNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

        var bestKey = "C"
        var bestScale = "Major"
        var bestCorrelation = -1.0

        for root in 0..<12 {
            var rotated = [Double](repeating: 0, count: 12)
            for i in 0..<12 {
                rotated[i] = histogram[(i + root) % 12]
            }

            let majorCorr = correlate(rotated, majorProfile)
            if majorCorr > bestCorrelation {
                bestCorrelation = majorCorr
                bestKey = noteNames[root]
                bestScale = "Major"
            }

            let minorCorr = correlate(rotated, minorProfile)
            if minorCorr > bestCorrelation {
                bestCorrelation = minorCorr
                bestKey = noteNames[root]
                bestScale = "Minor"
            }
        }

        let confidence = min(max((bestCorrelation + 1) / 2, 0), 1)

        return (bestKey, bestScale, confidence)
    }

    private static func correlate(_ a: [Double], _ b: [Double]) -> Double {
        guard a.count == b.count, !a.isEmpty else { return 0 }

        let n = Double(a.count)
        let meanA = a.reduce(0, +) / n
        let meanB = b.reduce(0, +) / n

        var num = 0.0, denA = 0.0, denB = 0.0

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
    @Published var selectedIndex: Int = -1
    @Published var isScanning: Bool = false
    @Published var scanProgress: Double = 0.0
    @Published var scanStatus: String = ""
    @Published var filterKey: String = "All"
    @Published var filterScale: String = "All"
    @Published var searchText: String = ""
    @Published var savedFolders: [String] = []
    @Published var previewOnSelect: Bool = true
    @Published var syncWithDAW: Bool = true
    @Published var projectTempo: Double = 120.0
    @Published var projectKey: String = "C"
    @Published var projectScale: String = "Major"

    let database: DatabaseManager
    let midiPlayer = MIDIPlayer()

    init() {
        let appSupport = FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        ).first!

        let appFolder = appSupport.appendingPathComponent("MIDIXplorer")
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

    func removeSavedFolder(_ path: String) {
        database.removeScanFolder(path)
        loadSavedFolders()
    }

    func startScan(paths: [URL]) {
        print("[Scan] startScan called with \(paths.count) paths: \(paths.map { $0.path })")
        isScanning = true
        scanProgress = 0.0
        scanStatus = "Discovering MIDI files..."

        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self = self else {
                print("[Scan] ERROR: self is nil in async block")
                return
            }

            var allMIDIFiles: [URL] = []

            for path in paths {
                print("[Scan] Adding scan folder: \(path.path)")
                self.database.addScanFolder(path.path)
                let files = MIDIScanner.scanFolder(url: path, recursive: true)
                print("[Scan] Found \(files.count) MIDI files in \(path.lastPathComponent)")
                allMIDIFiles.append(contentsOf: files)
            }

            let total = allMIDIFiles.count
            print("[Scan] Total MIDI files to analyze: \(total)")

            DispatchQueue.main.async {
                self.scanStatus = "Found \(total) MIDI files. Analyzing..."
            }

            for (index, fileURL) in allMIDIFiles.enumerated() {
                if let analyzed = MIDIScanner.analyzeMIDIFile(url: fileURL) {
                    self.database.addOrUpdateFile(analyzed)
                    print("[Scan] Analyzed: \(fileURL.lastPathComponent)")
                } else {
                    print("[Scan] Failed to analyze: \(fileURL.lastPathComponent)")
                }

                let progress = Double(index + 1) / Double(total)

                DispatchQueue.main.async {
                    self.scanProgress = progress
                    self.scanStatus = "Analyzing: \(fileURL.lastPathComponent)"
                }
            }

            DispatchQueue.main.async {
                print("[Scan] Scan complete. Reloading files...")
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
        return midiFiles.filter { file in
            let keyMatch = filterKey == "All" || file.detectedKey == filterKey
            let scaleMatch = filterScale == "All" || file.detectedScale.contains(filterScale)
            let searchMatch = searchText.isEmpty ||
                file.fileName.localizedCaseInsensitiveContains(searchText) ||
                file.filePath.localizedCaseInsensitiveContains(searchText)

            return keyMatch && scaleMatch && searchMatch
        }
    }

    func selectFile(_ file: MIDIFileModel) {
        selectedFile = file
        if let idx = filteredFiles().firstIndex(where: { $0.id == file.id }) {
            selectedIndex = idx
        }

        if previewOnSelect {
            playPreview(file: file)
        }
    }

    func selectPrevious() {
        let files = filteredFiles()
        guard !files.isEmpty else { return }

        if selectedIndex > 0 {
            selectedIndex -= 1
        } else {
            selectedIndex = files.count - 1
        }

        let file = files[selectedIndex]
        selectedFile = file

        if previewOnSelect {
            playPreview(file: file)
        }
    }

    func selectNext() {
        let files = filteredFiles()
        guard !files.isEmpty else { return }

        if selectedIndex < files.count - 1 {
            selectedIndex += 1
        } else {
            selectedIndex = 0
        }

        let file = files[selectedIndex]
        selectedFile = file

        if previewOnSelect {
            playPreview(file: file)
        }
    }

    func playPreview(file: MIDIFileModel) {
        midiPlayer.stop()

        let url = URL(fileURLWithPath: file.filePath)
        if midiPlayer.loadMIDIFile(url: url) {
            // Use project tempo if syncing with DAW
            let tempo = syncWithDAW ? projectTempo : file.tempo
            midiPlayer.play(atTempo: tempo)
            database.incrementPlayCount(id: file.id)
        }
    }

    func stopPreview() {
        midiPlayer.stop()
    }

    func toggleFavorite(file: MIDIFileModel) {
        database.toggleFavorite(id: file.id)
        loadFiles()
    }

    func setRating(file: MIDIFileModel, rating: Int) {
        database.setRating(id: file.id, rating: rating)
        loadFiles()
    }

    func deleteFile(file: MIDIFileModel) {
        database.deleteFile(id: file.id)
        if selectedFile?.id == file.id {
            selectedFile = nil
            selectedIndex = -1
        }
        loadFiles()
    }

    func rescanAllFolders() {
        let urls = savedFolders.map { URL(fileURLWithPath: $0) }
        startScan(paths: urls)
    }

    func setProjectKey(_ key: String, scale: String) {
        projectKey = key
        projectScale = scale
        filterKey = key
        filterScale = scale
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
    var timeSignature: String = "4/4"
    var lastModified: Double = 0
    var isFavorite: Bool = false
    var playCount: Int = 0
    var rating: Int = 0
    var tags: String = ""

    func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }

    static func == (lhs: MIDIFileModel, rhs: MIDIFileModel) -> Bool {
        lhs.id == rhs.id
    }
}

// MARK: - Drag and Drop Support
extension MIDIFileModel {
    func itemProvider() -> NSItemProvider {
        let url = URL(fileURLWithPath: filePath)
        let provider = NSItemProvider(contentsOf: url)
        return provider ?? NSItemProvider()
    }
}
