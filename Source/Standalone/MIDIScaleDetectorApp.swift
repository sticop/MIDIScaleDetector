import SwiftUI
import Combine

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

class AppState: ObservableObject {
    @Published var midiFiles: [MIDIFileModel] = []
    @Published var selectedFile: MIDIFileModel?
    @Published var isScanning: Bool = false
    @Published var scanProgress: Double = 0.0
    @Published var filterKey: String = "All"
    @Published var filterScale: String = "All"
    @Published var searchText: String = ""

    // Database path
    let databasePath: String

    init() {
        let appSupport = FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        ).first!

        let appFolder = appSupport.appendingPathComponent("MIDIScaleDetector")
        try? FileManager.default.createDirectory(at: appFolder, withIntermediateDirectories: true)

        databasePath = appFolder.appendingPathComponent("midi_library.db").path

        loadFiles()
    }

    func loadFiles() {
        // This will be implemented via C++ bridge
        // For now, sample data
    }

    func startScan(paths: [URL]) {
        isScanning = true
        scanProgress = 0.0

        // Scan implementation via C++ bridge
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
            self.isScanning = false
            self.loadFiles()
        }
    }

    func filteredFiles() -> [MIDIFileModel] {
        midiFiles.filter { file in
            let keyMatch = filterKey == "All" || file.detectedKey == filterKey
            let scaleMatch = filterScale == "All" || file.detectedScale == filterScale
            let searchMatch = searchText.isEmpty ||
                file.fileName.localizedCaseInsensitiveContains(searchText)

            return keyMatch && scaleMatch && searchMatch
        }
    }
}

struct MIDIFileModel: Identifiable {
    let id: Int
    let filePath: String
    let fileName: String
    let detectedKey: String
    let detectedScale: String
    let confidence: Double
    let tempo: Double
    let duration: Double
    let totalNotes: Int
}
