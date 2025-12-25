import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @EnvironmentObject var appState: AppState
    @State private var selectedSidebar: SidebarItem = .library
    @FocusState private var isFileListFocused: Bool

    var body: some View {
        NavigationView {
            // Sidebar
            SidebarView(selectedSidebar: $selectedSidebar)

            // Main Content
            VStack(spacing: 0) {
                // Project Settings Bar
                ProjectSettingsBar()
                
                Divider()
                
                // Toolbar
                ToolbarView()

                Divider()

                // File List or Scanning View
                if appState.isScanning {
                    ScanningView()
                } else if appState.midiFiles.isEmpty {
                    EmptyLibraryView()
                } else {
                    FileListView(isFileListFocused: $isFileListFocused)
                        .focused($isFileListFocused)
                }
                
                // Transport Controls
                TransportBar()
            }

            // Detail View
            if let selectedFile = appState.selectedFile {
                DetailView(file: selectedFile)
            } else {
                PlaceholderDetailView()
            }
        }
        .onAppear {
            isFileListFocused = true
        }
        .onChange(of: selectedSidebar) { oldValue, newValue in
            handleSidebarSelection(newValue)
        }
        // Global keyboard shortcuts
        .onKeyPress(.upArrow) {
            appState.selectPrevious()
            return .handled
        }
        .onKeyPress(.downArrow) {
            appState.selectNext()
            return .handled
        }
        .onKeyPress(.space) {
            if appState.midiPlayer.isPlaying {
                appState.stopPreview()
            } else if let file = appState.selectedFile {
                appState.playPreview(file: file)
            }
            return .handled
        }
        .onKeyPress(.escape) {
            appState.stopPreview()
            return .handled
        }
    }
    
    func handleSidebarSelection(_ item: SidebarItem) {
        switch item {
        case .library:
            appState.filterKey = "All"
            appState.filterScale = "All"
        case .favorites:
            break
        case .key(let key):
            appState.filterKey = key
            appState.filterScale = "All"
        case .scale(let scale):
            appState.filterKey = "All"
            appState.filterScale = scale
        }
    }
}

// MARK: - Sidebar
struct SidebarView: View {
    @EnvironmentObject var appState: AppState
    @Binding var selectedSidebar: SidebarItem
    
    var body: some View {
        List(selection: $selectedSidebar) {
            Section("Library") {
                Label("All Files (\(appState.midiFiles.count))", systemImage: "music.note.list")
                    .tag(SidebarItem.library)
                Label("Favorites", systemImage: "star.fill")
                    .tag(SidebarItem.favorites)
            }
            
            Section("Watched Folders") {
                ForEach(appState.savedFolders, id: \.self) { folder in
                    let folderExists = FileManager.default.fileExists(atPath: folder)
                    HStack {
                        Image(systemName: folderExists ? "folder" : "folder.badge.questionmark")
                            .foregroundColor(folderExists ? .primary : .orange)
                        Text(URL(fileURLWithPath: folder).lastPathComponent)
                            .foregroundColor(folderExists ? .primary : .secondary)
                        if !folderExists {
                            Text("Offline")
                                .font(.caption2)
                                .foregroundColor(.orange)
                                .padding(.horizontal, 4)
                                .padding(.vertical, 2)
                                .background(Color.orange.opacity(0.2))
                                .cornerRadius(4)
                        }
                    }
                    .contextMenu {
                        Button("Reveal in Finder") {
                            NSWorkspace.shared.selectFile(nil, inFileViewerRootedAtPath: folder)
                        }
                        .disabled(!folderExists)
                        if !folderExists {
                            Button("Remove from Library", role: .destructive) {
                                appState.removeSavedFolder(folder)
                            }
                        }
                    }
                }
            }

            Section("Filter by Key") {
                ForEach(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"], id: \.self) { key in
                    let count = appState.midiFiles.filter { $0.detectedKey == key }.count
                    if count > 0 {
                        HStack {
                            Text(key)
                            Spacer()
                            Text("\(count)")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                        .tag(SidebarItem.key(key))
                    }
                }
            }

            Section("Filter by Scale") {
                ForEach(["Major", "Minor"], id: \.self) { scale in
                    let count = appState.midiFiles.filter { $0.detectedScale == scale }.count
                    if count > 0 {
                        HStack {
                            Text(scale)
                            Spacer()
                            Text("\(count)")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                        .tag(SidebarItem.scale(scale))
                    }
                }
            }
        }
        .listStyle(SidebarListStyle())
        .frame(minWidth: 200)
    }
}

enum SidebarItem: Hashable {
    case library
    case favorites
    case key(String)
    case scale(String)
}

// MARK: - Project Settings Bar
struct ProjectSettingsBar: View {
    @EnvironmentObject var appState: AppState
    
    var body: some View {
        HStack(spacing: 16) {
            // Project Key/Scale
            HStack(spacing: 8) {
                Text("Project:")
                    .foregroundColor(.secondary)
                
                Picker("Key", selection: $appState.projectKey) {
                    ForEach(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"], id: \.self) { key in
                        Text(key).tag(key)
                    }
                }
                .frame(width: 70)
                
                Picker("Scale", selection: $appState.projectScale) {
                    ForEach(["Major", "Minor"], id: \.self) { scale in
                        Text(scale).tag(scale)
                    }
                }
                .frame(width: 80)
                
                Button("Match Filter") {
                    appState.filterKey = appState.projectKey
                    appState.filterScale = appState.projectScale
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
            }
            
            Divider().frame(height: 20)
            
            // Project Tempo
            HStack(spacing: 8) {
                Text("Tempo:")
                    .foregroundColor(.secondary)
                
                TextField("BPM", value: $appState.projectTempo, format: .number)
                    .frame(width: 60)
                    .textFieldStyle(.roundedBorder)
                
                Text("BPM")
                    .foregroundColor(.secondary)
            }
            
            Divider().frame(height: 20)
            
            // Sync Options
            Toggle("Sync Preview Tempo", isOn: $appState.syncWithDAW)
                .toggleStyle(.checkbox)
            
            Toggle("Auto-Preview", isOn: $appState.previewOnSelect)
                .toggleStyle(.checkbox)
            
            Spacer()
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(Color(NSColor.controlBackgroundColor))
    }
}

// MARK: - Placeholder Detail View
struct PlaceholderDetailView: View {
    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "music.note")
                .font(.system(size: 48))
                .foregroundColor(.secondary)
            Text("Select a MIDI file")
                .font(.title2)
                .foregroundColor(.secondary)
            Text("Use ↑↓ arrows to browse, Space to preview")
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

// MARK: - Empty Library View
struct EmptyLibraryView: View {
    @EnvironmentObject var appState: AppState
    @State private var showingScanDialog = false
    
    var body: some View {
        VStack(spacing: 24) {
            Image(systemName: "folder.badge.plus")
                .font(.system(size: 64))
                .foregroundColor(.accentColor)
            
            Text("No MIDI Files Found")
                .font(.title)
                .fontWeight(.semibold)
            
            Text("Scan a folder to discover and analyze your MIDI files")
                .foregroundColor(.secondary)
            
            Button(action: { showingScanDialog = true }) {
                Label("Add Folder to Scan", systemImage: "plus.circle.fill")
                    .font(.headline)
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
            
            VStack(alignment: .leading, spacing: 8) {
                Text("Common locations:")
                    .font(.caption)
                    .foregroundColor(.secondary)
                
                HStack {
                    QuickFolderButton(name: "Music", path: FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Music"))
                    QuickFolderButton(name: "Downloads", path: FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Downloads"))
                    QuickFolderButton(name: "Documents", path: FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Documents"))
                }
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .sheet(isPresented: $showingScanDialog) {
            ScanDialog()
        }
    }
}

struct QuickFolderButton: View {
    @EnvironmentObject var appState: AppState
    let name: String
    let path: URL
    
    var body: some View {
        Button(action: {
            appState.startScan(paths: [path])
        }) {
            Label(name, systemImage: "folder")
        }
        .buttonStyle(.bordered)
    }
}

// MARK: - Toolbar View
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
                
                if !appState.searchText.isEmpty {
                    Button(action: { appState.searchText = "" }) {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundColor(.secondary)
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(8)
            .background(Color(NSColor.controlBackgroundColor))
            .cornerRadius(6)
            .frame(width: 300)

            Spacer()
            
            // File count
            Text("\(appState.filteredFiles().count) files")
                .font(.caption)
                .foregroundColor(.secondary)
            
            Spacer()

            // Filter buttons
            Menu {
                Picker("Key", selection: $appState.filterKey) {
                    Text("All Keys").tag("All")
                    Divider()
                    ForEach(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"], id: \.self) { key in
                        Text(key).tag(key)
                    }
                }

                Picker("Scale", selection: $appState.filterScale) {
                    Text("All Scales").tag("All")
                    Divider()
                    ForEach(["Major", "Minor"], id: \.self) { scale in
                        Text(scale).tag(scale)
                    }
                }
            } label: {
                HStack {
                    Image(systemName: "line.3.horizontal.decrease.circle")
                    if appState.filterKey != "All" || appState.filterScale != "All" {
                        Text("\(appState.filterKey) \(appState.filterScale)")
                            .font(.caption)
                    }
                }
            }
            
            // Rescan button
            if !appState.savedFolders.isEmpty {
                Button(action: { appState.rescanAllFolders() }) {
                    Label("Rescan", systemImage: "arrow.clockwise")
                }
                .help("Rescan all watched folders")
            }

            // Scan button
            Button(action: { showingScanDialog = true }) {
                Label("Add Folder", systemImage: "folder.badge.plus")
            }
            .sheet(isPresented: $showingScanDialog) {
                ScanDialog()
            }
        }
        .padding()
    }
}

// MARK: - Transport Bar
struct TransportBar: View {
    @EnvironmentObject var appState: AppState
    
    var body: some View {
        HStack(spacing: 16) {
            // Play/Stop
            Button(action: {
                if appState.midiPlayer.isPlaying {
                    appState.stopPreview()
                } else if let file = appState.selectedFile {
                    appState.playPreview(file: file)
                }
            }) {
                Image(systemName: appState.midiPlayer.isPlaying ? "stop.fill" : "play.fill")
                    .font(.title2)
            }
            .buttonStyle(.plain)
            .disabled(appState.selectedFile == nil)
            
            // Now playing info
            if let file = appState.selectedFile {
                VStack(alignment: .leading, spacing: 2) {
                    Text(file.fileName)
                        .font(.caption)
                        .lineLimit(1)
                    HStack(spacing: 8) {
                        Text("\(file.detectedKey) \(file.detectedScale)")
                            .font(.caption2)
                            .foregroundColor(.accentColor)
                        Text("•")
                            .foregroundColor(.secondary)
                        Text("\(Int(appState.syncWithDAW ? appState.projectTempo : file.tempo)) BPM")
                            .font(.caption2)
                            .foregroundColor(.secondary)
                    }
                }
            } else {
                Text("No file selected")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            Spacer()
            
            // Navigation hint
            HStack(spacing: 4) {
                Image(systemName: "arrow.up")
                Image(systemName: "arrow.down")
                Text("to browse")
                    .font(.caption2)
                Text("•")
                Image(systemName: "space")
                Text("to play/stop")
                    .font(.caption2)
            }
            .foregroundColor(.secondary)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(Color(NSColor.controlBackgroundColor))
            .cornerRadius(4)
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(Color(NSColor.windowBackgroundColor))
    }
}

// MARK: - File List View with Drag Support
struct FileListView: View {
    @EnvironmentObject var appState: AppState
    @FocusState.Binding var isFileListFocused: Bool
    @State private var sortOrder = [KeyPathComparator(\MIDIFileModel.fileName)]

    var body: some View {
        Table(appState.filteredFiles(), selection: Binding(
            get: { appState.selectedFile?.id },
            set: { id in
                if let id = id, let file = appState.midiFiles.first(where: { $0.id == id }) {
                    appState.selectFile(file)
                }
            }
        ), sortOrder: $sortOrder) {
            TableColumn("★") { file in
                Button(action: { appState.toggleFavorite(file: file) }) {
                    Image(systemName: file.isFavorite ? "star.fill" : "star")
                        .foregroundColor(file.isFavorite ? .yellow : .gray)
                }
                .buttonStyle(.plain)
            }
            .width(30)
            
            TableColumn("File Name") { file in
                // Draggable file name
                Text(file.fileName)
                    .draggable(file.itemProvider()) {
                        // Drag preview
                        HStack {
                            Image(systemName: "music.note")
                            Text(file.fileName)
                        }
                        .padding(8)
                        .background(Color.accentColor.opacity(0.2))
                        .cornerRadius(8)
                    }
            }
            .width(min: 200)

            TableColumn("Key") { file in
                Text(file.detectedKey)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 2)
                    .background(keyColor(file.detectedKey).opacity(0.2))
                    .cornerRadius(4)
            }
            .width(60)

            TableColumn("Scale", value: \.detectedScale)
                .width(80)

            TableColumn("Confidence") { file in
                HStack(spacing: 4) {
                    ConfidenceBar(value: file.confidence)
                    Text("\(Int(file.confidence * 100))%")
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .frame(width: 35, alignment: .trailing)
                }
            }
            .width(100)

            TableColumn("BPM") { file in
                Text(String(format: "%.0f", file.tempo))
            }
            .width(50)
            
            TableColumn("Time") { file in
                Text(file.timeSignature)
            }
            .width(45)

            TableColumn("Duration") { file in
                Text(formatDuration(file.duration))
            }
            .width(60)

            TableColumn("Plays") { file in
                Text("\(file.playCount)")
                    .foregroundColor(.secondary)
            }
            .width(45)
        }
        .contextMenu(forSelectionType: Int.self) { items in
            if let id = items.first, let file = appState.midiFiles.first(where: { $0.id == id }) {
                Button("Preview") {
                    appState.playPreview(file: file)
                }
                Button("Show in Finder") {
                    NSWorkspace.shared.selectFile(file.filePath, inFileViewerRootedAtPath: "")
                }
                Divider()
                Button(file.isFavorite ? "Remove from Favorites" : "Add to Favorites") {
                    appState.toggleFavorite(file: file)
                }
                Divider()
                Button("Remove from Library", role: .destructive) {
                    appState.deleteFile(file: file)
                }
            }
        } primaryAction: { items in
            if let id = items.first, let file = appState.midiFiles.first(where: { $0.id == id }) {
                appState.playPreview(file: file)
            }
        }
    }

    func formatDuration(_ seconds: Double) -> String {
        let mins = Int(seconds) / 60
        let secs = Int(seconds) % 60
        return String(format: "%d:%02d", mins, secs)
    }
    
    func keyColor(_ key: String) -> Color {
        let keyColors: [String: Color] = [
            "C": .blue, "C#": .purple, "D": .green, "D#": .teal,
            "E": .orange, "F": .red, "F#": .pink, "G": .yellow,
            "G#": .mint, "A": .indigo, "A#": .brown, "B": .cyan
        ]
        return keyColors[key] ?? .gray
    }
}

// MARK: - Confidence Bar
struct ConfidenceBar: View {
    let value: Double
    
    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .leading) {
                Rectangle()
                    .fill(Color.gray.opacity(0.2))
                    .cornerRadius(2)
                
                Rectangle()
                    .fill(confidenceColor)
                    .frame(width: geo.size.width * value)
                    .cornerRadius(2)
            }
        }
        .frame(height: 6)
    }
    
    var confidenceColor: Color {
        if value >= 0.8 { return .green }
        if value >= 0.6 { return .yellow }
        return .orange
    }
}

// MARK: - Detail View
struct DetailView: View {
    @EnvironmentObject var appState: AppState
    let file: MIDIFileModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                // Header with drag hint
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        VStack(alignment: .leading) {
                            Text(file.fileName)
                                .font(.title)
                                .fontWeight(.bold)
                            
                            Text("Drag to DAW to import")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                        
                        Spacer()
                        
                        // Draggable icon
                        Image(systemName: "arrow.up.forward.app")
                            .font(.title)
                            .foregroundColor(.accentColor)
                            .draggable(file.itemProvider()) {
                                HStack {
                                    Image(systemName: "music.note")
                                    Text(file.fileName)
                                }
                                .padding()
                                .background(Color.accentColor)
                                .foregroundColor(.white)
                                .cornerRadius(8)
                            }
                    }

                    Text(file.filePath)
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .lineLimit(2)
                }

                Divider()

                // Key/Scale Display
                HStack(spacing: 20) {
                    VStack {
                        Text(file.detectedKey)
                            .font(.system(size: 48, weight: .bold, design: .rounded))
                        Text(file.detectedScale)
                            .font(.title3)
                            .foregroundColor(.secondary)
                    }
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(Color.accentColor.opacity(0.1))
                    .cornerRadius(12)
                    
                    VStack(spacing: 8) {
                        CircularProgressView(value: file.confidence)
                            .frame(width: 80, height: 80)
                        Text("Confidence")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    .padding()
                }

                // Musical Properties
                GroupBox("Musical Properties") {
                    VStack(alignment: .leading, spacing: 12) {
                        PropertyRow(label: "Detected Key", value: file.detectedKey)
                        PropertyRow(label: "Scale Type", value: file.detectedScale)
                        PropertyRow(label: "Confidence", value: "\(Int(file.confidence * 100))%")
                        PropertyRow(label: "Original Tempo", value: String(format: "%.0f BPM", file.tempo))
                        PropertyRow(label: "Time Signature", value: file.timeSignature)
                        PropertyRow(label: "Duration", value: formatDuration(file.duration))
                        PropertyRow(label: "Total Notes", value: "\(file.totalNotes)")
                        PropertyRow(label: "Play Count", value: "\(file.playCount)")
                    }
                    .padding()
                }

                // Scale Notes
                GroupBox("Scale Notes") {
                    HStack(spacing: 4) {
                        ForEach(getScaleNotes(root: file.detectedKey, scale: file.detectedScale), id: \.self) { note in
                            Text(note)
                                .font(.headline)
                                .padding(.horizontal, 12)
                                .padding(.vertical, 8)
                                .background(Color.accentColor.opacity(0.2))
                                .cornerRadius(6)
                        }
                    }
                    .padding()
                }

                // Actions
                HStack {
                    Button(action: { appState.playPreview(file: file) }) {
                        Label("Preview", systemImage: "play.fill")
                    }
                    .buttonStyle(.borderedProminent)
                    
                    Button("Show in Finder") {
                        NSWorkspace.shared.selectFile(file.filePath, inFileViewerRootedAtPath: "")
                    }
                    
                    Button("Copy Path") {
                        NSPasteboard.general.clearContents()
                        NSPasteboard.general.setString(file.filePath, forType: .string)
                    }
                    
                    Spacer()
                }

                Spacer()
            }
            .padding()
        }
        .frame(minWidth: 300)
    }
    
    func formatDuration(_ seconds: Double) -> String {
        let mins = Int(seconds) / 60
        let secs = Int(seconds) % 60
        return String(format: "%d:%02d", mins, secs)
    }
    
    func getScaleNotes(root: String, scale: String) -> [String] {
        let notes = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
        guard let rootIndex = notes.firstIndex(of: root) else { return [] }
        
        let intervals: [Int]
        switch scale {
        case "Major": intervals = [0, 2, 4, 5, 7, 9, 11]
        case "Minor": intervals = [0, 2, 3, 5, 7, 8, 10]
        default: intervals = [0, 2, 4, 5, 7, 9, 11]
        }
        
        return intervals.map { notes[(rootIndex + $0) % 12] }
    }
}

// MARK: - Circular Progress View
struct CircularProgressView: View {
    let value: Double
    
    var body: some View {
        ZStack {
            Circle()
                .stroke(Color.gray.opacity(0.2), lineWidth: 8)
            
            Circle()
                .trim(from: 0, to: value)
                .stroke(progressColor, style: StrokeStyle(lineWidth: 8, lineCap: .round))
                .rotationEffect(.degrees(-90))
            
            Text("\(Int(value * 100))%")
                .font(.headline)
        }
    }
    
    var progressColor: Color {
        if value >= 0.8 { return .green }
        if value >= 0.6 { return .yellow }
        return .orange
    }
}

// MARK: - Property Row
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

// MARK: - Scanning View
struct ScanningView: View {
    @EnvironmentObject var appState: AppState

    var body: some View {
        VStack(spacing: 20) {
            ProgressView(value: appState.scanProgress) {
                Text("Scanning MIDI files...")
                    .font(.headline)
            }
            .frame(width: 400)
            
            Text(appState.scanStatus)
                .font(.caption)
                .foregroundColor(.secondary)
                .lineLimit(1)
                .frame(width: 400)

            Text("\(Int(appState.scanProgress * 100))% Complete")
                .font(.title2)
                .fontWeight(.semibold)

            Button("Cancel") {
                appState.cancelScan()
            }
            .buttonStyle(.bordered)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

// MARK: - Scan Dialog
struct ScanDialog: View {
    @Environment(\.dismiss) var dismiss
    @EnvironmentObject var appState: AppState
    @State private var selectedPaths: [URL] = []
    @State private var scanRecursively = true

    var body: some View {
        VStack(spacing: 20) {
            Text("Scan for MIDI Files")
                .font(.title)
                .fontWeight(.semibold)

            VStack(alignment: .leading, spacing: 10) {
                Text("Select folders to scan:")
                    .font(.headline)

                if selectedPaths.isEmpty {
                    VStack {
                        Spacer()
                        Image(systemName: "folder.badge.plus")
                            .font(.largeTitle)
                            .foregroundColor(.secondary)
                        Text("No folders selected")
                            .foregroundColor(.secondary)
                        Spacer()
                    }
                    .frame(height: 150)
                    .frame(maxWidth: .infinity)
                    .background(Color(NSColor.controlBackgroundColor))
                    .cornerRadius(8)
                } else {
                    List {
                        ForEach(selectedPaths, id: \.self) { url in
                            HStack {
                                Image(systemName: "folder.fill")
                                    .foregroundColor(.accentColor)
                                VStack(alignment: .leading) {
                                    Text(url.lastPathComponent)
                                        .fontWeight(.medium)
                                    Text(url.path)
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                        .lineLimit(1)
                                }
                                Spacer()
                                Button(action: { selectedPaths.removeAll { $0 == url } }) {
                                    Image(systemName: "xmark.circle.fill")
                                        .foregroundColor(.secondary)
                                }
                                .buttonStyle(.plain)
                            }
                        }
                    }
                    .frame(height: 150)
                }

                HStack {
                    Button("Add Folder...") {
                        let panel = NSOpenPanel()
                        panel.canChooseFiles = false
                        panel.canChooseDirectories = true
                        panel.allowsMultipleSelection = true
                        panel.message = "Select folders containing MIDI files"
                        panel.prompt = "Add Folder"

                        if panel.runModal() == .OK {
                            for url in panel.urls {
                                if !selectedPaths.contains(url) {
                                    selectedPaths.append(url)
                                }
                            }
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    
                    Spacer()
                    
                    Toggle("Scan subfolders", isOn: $scanRecursively)
                }
            }

            Divider()
            
            if !appState.savedFolders.isEmpty {
                VStack(alignment: .leading) {
                    Text("Previously scanned folders:")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack {
                            ForEach(appState.savedFolders, id: \.self) { folder in
                                Button(action: {
                                    let url = URL(fileURLWithPath: folder)
                                    if !selectedPaths.contains(url) {
                                        selectedPaths.append(url)
                                    }
                                }) {
                                    Label(URL(fileURLWithPath: folder).lastPathComponent, systemImage: "folder")
                                }
                                .buttonStyle(.bordered)
                            }
                        }
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
                .buttonStyle(.borderedProminent)
                .disabled(selectedPaths.isEmpty)
            }
        }
        .padding()
        .frame(width: 550, height: 450)
    }
}

// MARK: - Settings Views
struct SettingsView: View {
    @State private var selection: SettingsSection = .general

    var body: some View {
        HStack(spacing: 0) {
            List(selection: $selection) {
                ForEach(SettingsSection.allCases) { section in
                    Label(section.title, systemImage: section.icon)
                        .tag(section)
                }
            }
            .listStyle(SidebarListStyle())
            .frame(minWidth: 180, maxWidth: 200)

            Divider()

            SettingsDetailView(section: selection)
        }
        .frame(width: 640, height: 440)
    }
}

enum SettingsSection: String, CaseIterable, Identifiable {
    case general
    case playback
    case plugin

    var id: String { rawValue }

    var title: String {
        switch self {
        case .general:
            return "General"
        case .playback:
            return "Playback"
        case .plugin:
            return "Plugin"
        }
    }

    var subtitle: String {
        switch self {
        case .general:
            return "Storage and database options"
        case .playback:
            return "Preview and MIDI output behavior"
        case .plugin:
            return "Plugin locations and shortcuts"
        }
    }

    var icon: String {
        switch self {
        case .general:
            return "gearshape"
        case .playback:
            return "play.circle"
        case .plugin:
            return "puzzlepiece"
        }
    }
}

struct SettingsDetailView: View {
    let section: SettingsSection

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            SettingsHeaderView(
                title: section.title,
                subtitle: section.subtitle,
                icon: section.icon
            )

            switch section {
            case .general:
                GeneralSettings()
            case .playback:
                PlaybackSettings()
            case .plugin:
                PluginSettings()
            }

            Spacer()
        }
        .padding(24)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(Color(NSColor.windowBackgroundColor))
    }
}

struct SettingsHeaderView: View {
    let title: String
    let subtitle: String
    let icon: String

    var body: some View {
        HStack(spacing: 12) {
            ZStack {
                Circle()
                    .fill(Color.accentColor.opacity(0.15))
                Image(systemName: icon)
                    .foregroundColor(.accentColor)
            }
            .frame(width: 36, height: 36)

            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.title2.weight(.semibold))
                Text(subtitle)
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
        }
    }
}

struct SettingsCard<Content: View>: View {
    let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            content
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(Color(NSColor.controlBackgroundColor))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .stroke(Color(NSColor.separatorColor), lineWidth: 1)
        )
    }
}

struct GeneralSettings: View {
    @AppStorage("databaseLocation") var databaseLocation = ""

    var body: some View {
        let defaultPath = FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        ).first!.appendingPathComponent("MIDIXplorer")

        let resolvedPath = databaseLocation.isEmpty ? defaultPath.path : databaseLocation

        return VStack(alignment: .leading, spacing: 16) {
            SettingsCard {
                Text("Database")
                    .font(.headline)

                Text("MIDI analysis and metadata are stored locally on this Mac.")
                    .font(.subheadline)
                    .foregroundColor(.secondary)

                VStack(alignment: .leading, spacing: 6) {
                    Text("Location")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    Text(resolvedPath)
                        .font(.system(.caption, design: .monospaced))
                        .foregroundColor(.primary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }

                HStack {
                    Button("Reveal in Finder") {
                        NSWorkspace.shared.open(URL(fileURLWithPath: resolvedPath))
                    }
                    Button("Copy Path") {
                        NSPasteboard.general.clearContents()
                        NSPasteboard.general.setString(resolvedPath, forType: .string)
                    }
                    .buttonStyle(.bordered)
                }
            }
        }
    }
}

struct PlaybackSettings: View {
    @EnvironmentObject var appState: AppState
    
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            SettingsCard {
                Text("Preview")
                    .font(.headline)

                Toggle("Auto-preview on selection", isOn: $appState.previewOnSelect)
                Toggle("Sync preview tempo with project", isOn: $appState.syncWithDAW)
            }

            SettingsCard {
                Text("MIDI Output")
                    .font(.headline)

                Text("MIDI is sent via virtual port: \"MIDI Xplorer\".")
                    .font(.caption)
                    .foregroundColor(.secondary)
                Text("Route this to your DAW's MIDI input to preview with your instruments.")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
    }
}

struct PluginSettings: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            SettingsCard {
                Text("Installation Paths")
                    .font(.headline)

                VStack(alignment: .leading, spacing: 6) {
                    LabeledContent("VST3") {
                        Text("~/Library/Audio/Plug-Ins/VST3/")
                            .font(.system(.caption, design: .monospaced))
                            .foregroundColor(.secondary)
                    }
                    LabeledContent("AU") {
                        Text("~/Library/Audio/Plug-Ins/Components/")
                            .font(.system(.caption, design: .monospaced))
                            .foregroundColor(.secondary)
                    }
                }

                HStack {
                    Button("Reveal VST3") {
                        NSWorkspace.shared.open(
                            FileManager.default.homeDirectoryForCurrentUser
                                .appendingPathComponent("Library/Audio/Plug-Ins/VST3")
                        )
                    }
                    Button("Reveal AU") {
                        NSWorkspace.shared.open(
                            FileManager.default.homeDirectoryForCurrentUser
                                .appendingPathComponent("Library/Audio/Plug-Ins/Components")
                        )
                    }
                }
            }
        }
    }
}

// MARK: - Menu Commands
struct MenuCommands: Commands {
    let appState: AppState

    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("Scan for MIDI Files...") {
                NotificationCenter.default.post(name: .showScanDialog, object: nil)
            }
            .keyboardShortcut("n", modifiers: .command)
            
            Button("Rescan All Folders") {
                appState.rescanAllFolders()
            }
            .keyboardShortcut("r", modifiers: [.command, .shift])
            .disabled(appState.savedFolders.isEmpty)
        }
        
        CommandGroup(after: .pasteboard) {
            Divider()
            Button("Previous File") {
                appState.selectPrevious()
            }
            .keyboardShortcut(.upArrow, modifiers: [])
            
            Button("Next File") {
                appState.selectNext()
            }
            .keyboardShortcut(.downArrow, modifiers: [])
            
            Divider()
            
            Button("Play/Stop Preview") {
                if appState.midiPlayer.isPlaying {
                    appState.stopPreview()
                } else if let file = appState.selectedFile {
                    appState.playPreview(file: file)
                }
            }
            .keyboardShortcut(.space, modifiers: [])
        }
    }
}

extension Notification.Name {
    static let showScanDialog = Notification.Name("showScanDialog")
}

#Preview {
    ContentView()
        .environmentObject(AppState())
}
