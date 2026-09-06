import AppKit
import SwiftUI

final class StudioAppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldSaveApplicationState(_ app: NSApplication) -> Bool { false }
    func applicationShouldRestoreApplicationState(_ app: NSApplication) -> Bool { false }
}

@main
struct NeuroevoStudioApp: App {
    @NSApplicationDelegateAdaptor(StudioAppDelegate.self) private var appDelegate
    @StateObject private var model = StudioModel()

    var body: some Scene {
        WindowGroup("Neuroevo Studio") {
            ContentView()
                .environmentObject(model)
                .frame(minWidth: 1120, minHeight: 760)
        }
        .windowStyle(.hiddenTitleBar)
        .defaultSize(width: 1320, height: 860)
    }
}
