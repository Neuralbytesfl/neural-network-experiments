import AppKit
import SwiftUI

enum WorkflowPage: String, CaseIterable, Identifiable {
    case welcome = "Welcome"
    case dataLab = "Create Data"
    case prepare = "Prepare Data"
    case train = "Train Model"
    case useModel = "Use Model"
    case learn = "Learn"

    var id: String { rawValue }
    var symbol: String {
        switch self {
        case .welcome: return "sparkles"
        case .dataLab: return "tablecells.badge.ellipsis"
        case .prepare: return "wand.and.stars"
        case .train: return "point.3.connected.trianglepath.dotted"
        case .useModel: return "bolt.horizontal.circle"
        case .learn: return "book.closed"
        }
    }
    var step: String? {
        switch self {
        case .dataLab: return "1"
        case .prepare: return "2"
        case .train: return "3"
        case .useModel: return "4"
        default: return nil
        }
    }
}

struct ContentView: View {
    @EnvironmentObject private var model: StudioModel
    @State private var selection: WorkflowPage?

    init() {
        let arguments = CommandLine.arguments
        let requested = arguments.firstIndex(of: "--page").flatMap { index -> WorkflowPage? in
            guard index + 1 < arguments.count else { return nil }
            switch arguments[index + 1].lowercased() {
            case "data", "create": return .dataLab
            case "prepare", "clean": return .prepare
            case "train": return .train
            case "use", "predict": return .useModel
            case "learn", "help": return .learn
            default: return .welcome
            }
        } ?? .welcome
        _selection = State(initialValue: requested)
    }

    var body: some View {
        NavigationSplitView {
            sidebar.navigationSplitViewColumnWidth(min: 230, ideal: 250, max: 290)
        } detail: {
            Group {
                switch selection ?? .welcome {
                case .welcome: WelcomeView(selection: $selection)
                case .dataLab: DataLabView(selection: $selection)
                case .prepare: PrepareDataView(selection: $selection)
                case .train: TrainingView(selection: $selection)
                case .useModel: UseModelView(selection: $selection)
                case .learn: LearnView()
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color(nsColor: .windowBackgroundColor))
        }
    }

    private var sidebar: some View {
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                Image(nsImage: NSApp.applicationIconImage).resizable().frame(width: 44, height: 44)
                VStack(alignment: .leading, spacing: 1) {
                    Text("Neuroevo").font(.headline)
                    Text("Studio").font(.caption).foregroundStyle(.secondary)
                }
                Spacer()
            }.padding(18)

            List(selection: $selection) {
                Section("WORKSPACE") {
                    ForEach(WorkflowPage.allCases.filter { $0 != .learn }) { page in
                        Label {
                            HStack {
                                Text(page.rawValue)
                                Spacer()
                                if let step = page.step {
                                    Text(step).font(.caption2.bold()).foregroundStyle(.secondary)
                                        .frame(width: 20, height: 20).background(.quaternary, in: Circle())
                                }
                            }
                        } icon: { Image(systemName: page.symbol) }
                        .tag(page)
                    }
                }
                Section("GUIDANCE") {
                    Label(WorkflowPage.learn.rawValue, systemImage: WorkflowPage.learn.symbol).tag(WorkflowPage.learn)
                }
            }.listStyle(.sidebar)

            VStack(alignment: .leading, spacing: 6) {
                Label(model.isTraining ? "Evolution running" : "Ready",
                      systemImage: model.isTraining ? "circle.dotted" : "checkmark.circle")
                    .font(.caption.bold()).foregroundStyle(model.isTraining ? .mint : .secondary)
                Text(model.backend).font(.caption2).foregroundStyle(.tertiary).lineLimit(2)
            }.padding(16).frame(maxWidth: .infinity, alignment: .leading)
        }
    }
}

struct PageHeader: View {
    let eyebrow: String
    let title: String
    let subtitle: String
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(eyebrow.uppercased()).font(.caption.bold()).foregroundStyle(.mint)
            Text(title).font(.system(size: 34, weight: .bold, design: .rounded))
            Text(subtitle).font(.title3).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }.frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct StudioPanel<Content: View>: View {
    let title: String
    let subtitle: String
    @ViewBuilder let content: Content
    init(_ title: String, subtitle: String, @ViewBuilder content: () -> Content) {
        self.title = title; self.subtitle = subtitle; self.content = content()
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.headline)
                Text(subtitle).font(.caption).foregroundStyle(.secondary)
            }
            content
        }
        .padding(18).frame(maxWidth: .infinity, alignment: .leading)
        .background(Color(nsColor: .controlBackgroundColor), in: RoundedRectangle(cornerRadius: 16))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(.separator.opacity(0.35)))
    }
}

struct HelpNote: View {
    let title: String
    let text: String
    var body: some View {
        HStack(alignment: .top, spacing: 11) {
            Image(systemName: "lightbulb.max.fill").foregroundStyle(.yellow)
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.subheadline.bold())
                Text(text).font(.caption).foregroundStyle(.secondary)
            }
        }
        .padding(13).frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.yellow.opacity(0.07), in: RoundedRectangle(cornerRadius: 12))
    }
}

struct MetricCard: View {
    let title: String
    let value: String
    let symbol: String
    var color: Color = .mint
    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 7) {
                Text(title.uppercased()).font(.caption2.bold()).foregroundStyle(.secondary)
                Text(value).font(.title2.bold().monospacedDigit()).lineLimit(1).minimumScaleFactor(0.55)
            }
            Spacer()
            Image(systemName: symbol).font(.title2).foregroundStyle(color)
        }
        .padding(16).background(Color(nsColor: .controlBackgroundColor), in: RoundedRectangle(cornerRadius: 14))
    }
}

struct PathButton: View {
    let title: String
    let icon: String
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            Label(title, systemImage: icon).lineLimit(1).frame(maxWidth: .infinity, alignment: .leading)
        }.buttonStyle(.bordered)
    }
}

struct NumberSetting: View {
    let title: String
    @Binding var value: Int
    var range: ClosedRange<Int> = 0...1_000_000
    var body: some View {
        HStack {
            Text(title); Spacer()
            TextField(title, value: $value, format: .number).frame(width: 90)
                .onChange(of: value) { _, newValue in value = min(range.upperBound, max(range.lowerBound, newValue)) }
        }
    }
}

struct DoubleSetting: View {
    let title: String
    @Binding var value: Double
    var range: ClosedRange<Double>
    var body: some View {
        HStack {
            Text(title); Spacer()
            TextField(title, value: $value, format: .number.precision(.fractionLength(0...6)))
                .frame(width: 100)
                .onChange(of: value) { _, newValue in
                    value = min(range.upperBound, max(range.lowerBound, newValue))
                }
        }
    }
}

struct FilePathRow: View {
    let label: String
    let path: String
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label.uppercased()).font(.caption2.bold()).foregroundStyle(.secondary)
            Text(path.isEmpty ? "Not selected" : path)
                .font(.caption.monospaced()).foregroundStyle(path.isEmpty ? .tertiary : .secondary)
                .lineLimit(2).textSelection(.enabled)
        }.frame(maxWidth: .infinity, alignment: .leading)
    }
}
