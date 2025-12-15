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
                
                Section("Watched Folders") {
                    ForEach(appState.savedFolders, id: \.self) { folder in
                        HStack {
                            Image(systemName: "folder")
                            Text(URL(fileURLWithPath: folder).lastPathComponent)
                        }
                        .contextMenu {
                            Button("Remove Folder") {
                                // Remove folder from watch list
                            }
                            Button("Reveal in Finder") {
                                NSWorkspace.shared.selectFile(nil, inFileViewerRootedAtPath: folder)
                            }
                        }
                    }
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
            .toolbar {
                ToolbarItem(placement: .automatic) {
                    Button(action: { NSApp.keyWindow?.firstResponder?.tryToPerform(#selector(NSSplitViewController.toggleSidebar(_:)), with: nil) }) {
                        Image(systemName: "sidebar.left")
                    }
                }
            }

            // Main Content
            VStack(spacing: 0) {
                // Toolbar
                ToolbarView()

                Divider()

                // File List or Scanning View
                if appState.isScanning {
                    ScanningView()
                } else if appState.midiFiles.isEmpty {
                    EmptyLibraryView()
                } else {
                    FileListView()
                }
            }

            // Detail View
            if let selectedFile = appState.selectedFile {
                DetailView(file: selectedFile)
            } else {
                VStack(spacing: 16) {
                    Image(systemName: "music.note")
                        .font(.system(size: 48))
                        .foregroundColor(.secondary)
                    Text("Select a MIDI file")
                        .font(.title2)
                        .foregroundColor(.secondary)
                    Text("Or scan a folder to get started")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .onChange(of: selectedSidebar) { oldValue, newValue in
            handleSidebarSelection(newValue)
        }
    }
    
    func handleSidebarSelection(_ item: SidebarItem) {
        switch item {
        case .library:
            appState.filterKey = "All"
            appState.filterScale = "All"
        case .favorites:
            // Filter to favorites only
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

enum SidebarItem: Hashable {
    case library
    case favorites
    case key(String)
    case scale(String)
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
            
            // Quick actions
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
                    ForEach(["Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian"], id: \.self) { scale in
                        Text(scale).tag(scale)
                    }
                }
            } label: {
                Label("Filter", systemImage: "line.3.horizontal.decrease.circle")
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

// MARK: - File List View
struct FileListView: View {
    @EnvironmentObject var appState: AppState
    @State private var sortOrder = [KeyPathComparator(\MIDIFileModel.fileName)]

    var body: some View {
        Table(appState.filteredFiles(), selection: Binding(
            get: { appState.selectedFile?.id },
            set: { id in
                appState.selectedFile = appState.midiFiles.first { $0.id == id }
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
            
            TableColumn("File Name", value: \.fileName)
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
                .width(100)

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

            TableColumn("Duration") { file in
                Text(formatDuration(file.duration))
            }
            .width(70)

            TableColumn("Notes") { file in
                Text("\(file.totalNotes)")
            }
            .width(50)
        }
        .contextMenu(forSelectionType: Int.self) { items in
            if let id = items.first, let file = appState.midiFiles.first(where: { $0.id == id }) {
                Button("Show in Finder") {
                    NSWorkspace.shared.selectFile(file.filePath, inFileViewerRootedAtPath: "")
                }
                Button(file.isFavorite ? "Remove from Favorites" : "Add to Favorites") {
                    appState.toggleFavorite(file: file)
                }
                Divider()
                Button("Remove from Library", role: .destructive) {
                    appState.deleteFile(file: file)
                }
            }
        } primaryAction: { items in
            if let id = items.first {
                appState.selectedFile = appState.midiFiles.first { $0.id == id }
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
                // Header
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text(file.fileName)
                            .font(.title)
                            .fontWeight(.bold)
                        
                        Spacer()
                        
                        Button(action: { appState.toggleFavorite(file: file) }) {
                            Image(systemName: file.isFavorite ? "star.fill" : "star")
                                .foregroundColor(file.isFavorite ? .yellow : .gray)
                                .font(.title2)
                        }
                        .buttonStyle(.plain)
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
                        PropertyRow(label: "Tempo", value: String(format: "%.0f BPM", file.tempo))
                        PropertyRow(label: "Duration", value: formatDuration(file.duration))
                        PropertyRow(label: "Total Notes", value: "\(file.totalNotes)")
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
                    Button("Show in Finder") {
                        NSWorkspace.shared.selectFile(file.filePath, inFileViewerRootedAtPath: "")
                    }
                    
                    Button("Copy Path") {
                        NSPasteboard.general.clearContents()
                        NSPasteboard.general.setString(file.filePath, forType: .string)
                    }
                    
                    Spacer()
                    
                    Button("Remove from Library", role: .destructive) {
                        appState.deleteFile(file: file)
                    }
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
        case "Dorian": intervals = [0, 2, 3, 5, 7, 9, 10]
        case "Phrygian": intervals = [0, 1, 3, 5, 7, 8, 10]
        case "Lydian": intervals = [0, 2, 4, 6, 7, 9, 11]
        case "Mixolydian": intervals = [0, 2, 4, 5, 7, 9, 10]
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

                // Folder list
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
            
            // Previously scanned folders
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
                LabeledContent("Location:") {
                    Text(databaseLocation.isEmpty ? "Default (Application Support)" : databaseLocation)
                        .foregroundColor(.secondary)
                }
                Button("Choose Location...") {
                    let panel = NSOpenPanel()
                    panel.canChooseFiles = false
                    panel.canChooseDirectories = true
                    if panel.runModal() == .OK, let url = panel.url {
                        databaseLocation = url.path
                    }
                }
                
                Button("Open Database Location") {
                    let path = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
                        .appendingPathComponent("MIDIScaleDetector")
                    NSWorkspace.shared.open(path)
                }
            }
        }
        .padding()
    }
}

struct ScanSettings: View {
    @AppStorage("scanRecursive") var scanRecursive = true
    @AppStorage("rescanModified") var rescanModified = true
    @AppStorage("watchFolders") var watchFolders = true

    var body: some View {
        Form {
            Toggle("Scan folders recursively", isOn: $scanRecursive)
            Toggle("Automatically rescan modified files", isOn: $rescanModified)
            Toggle("Watch folders for new files", isOn: $watchFolders)
        }
        .padding()
    }
}

struct PluginSettings: View {
    var body: some View {
        Form {
            Section("VST3/AU Plugin") {
                Text("Plugin installation location:")
                    .font(.headline)
                
                VStack(alignment: .leading, spacing: 4) {
                    HStack {
                        Text("VST3:")
                        Text("~/Library/Audio/Plug-Ins/VST3/")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("AU:")
                        Text("~/Library/Audio/Plug-Ins/Components/")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }

                HStack {
                    Button("Reveal VST3 in Finder") {
                        let pluginPath = FileManager.default.homeDirectoryForCurrentUser
                            .appendingPathComponent("Library/Audio/Plug-Ins/VST3")
                        NSWorkspace.shared.open(pluginPath)
                    }
                    
                    Button("Reveal AU in Finder") {
                        let pluginPath = FileManager.default.homeDirectoryForCurrentUser
                            .appendingPathComponent("Library/Audio/Plug-Ins/Components")
                        NSWorkspace.shared.open(pluginPath)
                    }
                }
            }
        }
        .padding()
    }
}

// MARK: - Menu Commands
struct MenuCommands: Commands {
    let appState: AppState
    @State private var showingScanDialog = false

    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("Scan for MIDI Files...") {
                // Post notification to trigger scan dialog
                NotificationCenter.default.post(name: .showScanDialog, object: nil)
            }
            .keyboardShortcut("n", modifiers: .command)
            
            Button("Rescan All Folders") {
                appState.rescanAllFolders()
            }
            .keyboardShortcut("r", modifiers: [.command, .shift])
            .disabled(appState.savedFolders.isEmpty)
        }

        CommandGroup(after: .importExport) {
            Button("Import MIDI File...") {
                let panel = NSOpenPanel()
                panel.allowedContentTypes = [.midi]
                panel.allowsMultipleSelection = true
                
                if panel.runModal() == .OK {
                    appState.startScan(paths: panel.urls.map { $0.deletingLastPathComponent() })
                }
            }
            .keyboardShortcut("i", modifiers: .command)
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
