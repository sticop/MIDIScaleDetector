import SwiftUI

struct ContentView: View {
    @EnvironmentObject var appState: AppState
    @State private var selectedSidebar: SidebarItem = .library

    var body: some View {
        NavigationView {
            // Sidebar
            List(selection: $selectedSidebar) {
                Section("Library") {
                    Label("All Files", systemImage: "music.note.list")
                        .tag(SidebarItem.library)
                    Label("Favorites", systemImage: "star")
                        .tag(SidebarItem.favorites)
                }

                Section("Keys") {
                    ForEach(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"], id: \.self) { key in
                        Text(key)
                            .tag(SidebarItem.key(key))
                    }
                }

                Section("Scales") {
                    ForEach(["Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian"], id: \.self) { scale in
                        Text(scale)
                            .tag(SidebarItem.scale(scale))
                    }
                }
            }
            .listStyle(SidebarListStyle())
            .frame(minWidth: 200)

            // Main Content
            VStack(spacing: 0) {
                // Toolbar
                ToolbarView()

                Divider()

                // File List
                if appState.isScanning {
                    ScanningView()
                } else {
                    FileListView()
                }
            }

            // Detail View
            if let selectedFile = appState.selectedFile {
                DetailView(file: selectedFile)
            } else {
                Text("Select a MIDI file")
                    .foregroundColor(.secondary)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
    }
}

enum SidebarItem: Hashable {
    case library
    case favorites
    case key(String)
    case scale(String)
}

struct ToolbarView: View {
    @EnvironmentObject var appState: AppState
    @State private var showingScanDialog = false

    var body: some View {
        HStack {
            // Search
            HStack {
                Image(systemName: "magnifyingglass")
                    .foregroundColor(.secondary)
                TextField("Search MIDI files...", text: $appState.searchText)
                    .textFieldStyle(PlainTextFieldStyle())
            }
            .padding(8)
            .background(Color(NSColor.controlBackgroundColor))
            .cornerRadius(6)
            .frame(width: 300)

            Spacer()

            // Filter buttons
            Menu {
                Picker("Key", selection: $appState.filterKey) {
                    Text("All Keys").tag("All")
                    ForEach(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"], id: \.self) { key in
                        Text(key).tag(key)
                    }
                }

                Picker("Scale", selection: $appState.filterScale) {
                    Text("All Scales").tag("All")
                    ForEach(["Major", "Minor", "Dorian", "Phrygian"], id: \.self) { scale in
                        Text(scale).tag(scale)
                    }
                }
            } label: {
                Label("Filter", systemImage: "line.3.horizontal.decrease.circle")
            }

            // Scan button
            Button(action: { showingScanDialog = true }) {
                Label("Scan", systemImage: "arrow.clockwise")
            }
            .sheet(isPresented: $showingScanDialog) {
                ScanDialog()
            }
        }
        .padding()
    }
}

struct FileListView: View {
    @EnvironmentObject var appState: AppState

    var body: some View {
        Table(of: MIDIFileModel.self) {
            TableColumn("File Name", value: \.fileName)
                .width(min: 200)

            TableColumn("Key") { file in
                Text(file.detectedKey)
                    .foregroundColor(.blue)
            }
            .width(50)

            TableColumn("Scale", value: \.detectedScale)
                .width(100)

            TableColumn("Confidence") { file in
                HStack {
                    ProgressView(value: file.confidence)
                        .frame(width: 60)
                    Text("\(Int(file.confidence * 100))%")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }
            .width(120)

            TableColumn("BPM") { file in
                Text(String(format: "%.1f", file.tempo))
            }
            .width(60)

            TableColumn("Duration") { file in
                Text(formatDuration(file.duration))
            }
            .width(80)

            TableColumn("Notes") { file in
                Text("\(file.totalNotes)")
            }
            .width(60)
        } rows: {
            ForEach(appState.filteredFiles()) { file in
                TableRow(file)
                    .onTapGesture {
                        appState.selectedFile = file
                    }
            }
        }
    }

    func formatDuration(_ seconds: Double) -> String {
        let mins = Int(seconds) / 60
        let secs = Int(seconds) % 60
        return String(format: "%d:%02d", mins, secs)
    }
}

struct DetailView: View {
    let file: MIDIFileModel

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            // Header
            VStack(alignment: .leading, spacing: 8) {
                Text(file.fileName)
                    .font(.title)
                    .fontWeight(.bold)

                Text(file.filePath)
                    .font(.caption)
                    .foregroundColor(.secondary)
            }

            Divider()

            // Musical Properties
            GroupBox("Musical Properties") {
                VStack(alignment: .leading, spacing: 12) {
                    PropertyRow(label: "Detected Key", value: file.detectedKey)
                    PropertyRow(label: "Scale Type", value: file.detectedScale)
                    PropertyRow(label: "Confidence", value: "\(Int(file.confidence * 100))%")
                    PropertyRow(label: "Tempo", value: String(format: "%.1f BPM", file.tempo))
                    PropertyRow(label: "Duration", value: String(format: "%.2f seconds", file.duration))
                    PropertyRow(label: "Total Notes", value: "\(file.totalNotes)")
                }
                .padding()
            }

            // Piano Roll Preview
            GroupBox("Note Distribution") {
                PianoRollView()
                    .frame(height: 200)
            }

            // Actions
            HStack {
                Button("Play Preview") {
                    // Play MIDI file
                }
                .buttonStyle(.borderedProminent)

                Button("Export to DAW") {
                    // Export functionality
                }

                Button("Show in Finder") {
                    NSWorkspace.shared.selectFile(file.filePath, inFileViewerRootedAtPath: "")
                }
            }

            Spacer()
        }
        .padding()
        .frame(minWidth: 300)
    }
}

struct PropertyRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(label + ":")
                .foregroundColor(.secondary)
            Spacer()
            Text(value)
                .fontWeight(.medium)
        }
    }
}

struct PianoRollView: View {
    var body: some View {
        // Simplified piano roll visualization
        GeometryReader { geometry in
            ZStack {
                // Background
                Color(NSColor.controlBackgroundColor)

                // Piano keys on left
                HStack(spacing: 0) {
                    VStack(spacing: 0) {
                        ForEach(0..<12) { note in
                            Rectangle()
                                .fill(isBlackKey(note) ? Color.black : Color.white)
                                .border(Color.gray, width: 0.5)
                        }
                    }
                    .frame(width: 30)

                    // Note grid
                    Rectangle()
                        .fill(Color.clear)
                        .overlay(
                            Text("Note visualization")
                                .foregroundColor(.secondary)
                        )
                }
            }
        }
    }

    func isBlackKey(_ note: Int) -> Bool {
        let blackKeys = [1, 3, 6, 8, 10]
        return blackKeys.contains(note)
    }
}

struct ScanningView: View {
    @EnvironmentObject var appState: AppState

    var body: some View {
        VStack(spacing: 20) {
            ProgressView(value: appState.scanProgress) {
                Text("Scanning MIDI files...")
            }
            .frame(width: 300)

            Text("\(Int(appState.scanProgress * 100))% Complete")
                .foregroundColor(.secondary)

            Button("Cancel") {
                // Cancel scan
                appState.isScanning = false
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

struct ScanDialog: View {
    @Environment(\.dismiss) var dismiss
    @EnvironmentObject var appState: AppState
    @State private var selectedPaths: [URL] = []

    var body: some View {
        VStack(spacing: 20) {
            Text("Scan for MIDI Files")
                .font(.title)

            VStack(alignment: .leading, spacing: 10) {
                Text("Select folders to scan:")
                    .font(.headline)

                List {
                    ForEach(selectedPaths, id: \.self) { url in
                        HStack {
                            Image(systemName: "folder")
                            Text(url.path)
                            Spacer()
                            Button(action: { selectedPaths.removeAll { $0 == url } }) {
                                Image(systemName: "xmark.circle")
                            }
                            .buttonStyle(.plain)
                        }
                    }
                }
                .frame(height: 150)

                Button("Add Folder...") {
                    let panel = NSOpenPanel()
                    panel.canChooseFiles = false
                    panel.canChooseDirectories = true
                    panel.allowsMultipleSelection = true

                    if panel.runModal() == .OK {
                        selectedPaths.append(contentsOf: panel.urls)
                    }
                }
            }

            HStack {
                Button("Cancel") {
                    dismiss()
                }
                .keyboardShortcut(.cancelAction)

                Spacer()

                Button("Start Scan") {
                    appState.startScan(paths: selectedPaths)
                    dismiss()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(selectedPaths.isEmpty)
            }
        }
        .padding()
        .frame(width: 500, height: 350)
    }
}

struct SettingsView: View {
    var body: some View {
        TabView {
            GeneralSettings()
                .tabItem {
                    Label("General", systemImage: "gear")
                }

            ScanSettings()
                .tabItem {
                    Label("Scanning", systemImage: "arrow.clockwise")
                }

            PluginSettings()
                .tabItem {
                    Label("Plugin", systemImage: "app.connected.to.app.below.fill")
                }
        }
        .frame(width: 500, height: 400)
    }
}

struct GeneralSettings: View {
    @AppStorage("databaseLocation") var databaseLocation = ""

    var body: some View {
        Form {
            Section("Database") {
                TextField("Location:", text: $databaseLocation)
                Button("Choose Location...") {
                    // File picker
                }
            }
        }
        .padding()
    }
}

struct ScanSettings: View {
    @AppStorage("scanRecursive") var scanRecursive = true
    @AppStorage("rescanModified") var rescanModified = true

    var body: some View {
        Form {
            Toggle("Scan folders recursively", isOn: $scanRecursive)
            Toggle("Rescan modified files", isOn: $rescanModified)
        }
        .padding()
    }
}

struct PluginSettings: View {
    var body: some View {
        Form {
            Section("VST3/AU Plugin") {
                Text("Plugin installation location:")
                Text("~/Library/Audio/Plug-Ins/VST3/")
                    .font(.caption)
                    .foregroundColor(.secondary)

                Button("Reveal in Finder") {
                    let pluginPath = FileManager.default.homeDirectoryForCurrentUser
                        .appendingPathComponent("Library/Audio/Plug-Ins/VST3")
                    NSWorkspace.shared.open(pluginPath)
                }
            }
        }
        .padding()
    }
}

struct MenuCommands: Commands {
    let appState: AppState

    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("Scan for MIDI Files...") {
                // Trigger scan
            }
            .keyboardShortcut("s", modifiers: .command)
        }

        CommandGroup(after: .importExport) {
            Button("Import MIDI File...") {
                // Import single file
            }
            .keyboardShortcut("i", modifiers: .command)
        }
    }
}

#Preview {
    ContentView()
        .environmentObject(AppState())
}
