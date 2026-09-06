import SwiftUI

struct WelcomeView: View {
    @Binding var selection: WorkflowPage?
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 26) {
                PageHeader(eyebrow: "Build machine learning you can understand",
                           title: "From an idea to a working model",
                           subtitle: "Create or import data, prepare it carefully, evolve a neural network, and use the result—all in one guided workspace.")
                HStack(spacing: 14) {
                    workflowCard("1", "Create Data", "Generate a known pattern or bring your own numeric CSV.", "tablecells.badge.ellipsis", .indigo, .dataLab)
                    workflowCard("2", "Prepare", "Find missing values, duplicates, malformed rows, and outliers.", "wand.and.stars", .cyan, .prepare)
                    workflowCard("3", "Train", "Watch candidate networks compete and evolve in real time.", "point.3.connected.trianglepath.dotted", .mint, .train)
                    workflowCard("4", "Use", "Predict new values and export the winner to Core ML.", "bolt.horizontal.circle", .orange, .useModel)
                }
                StudioPanel("Choose your starting point", subtitle: "Learn with synthetic data or begin with a real dataset") {
                    HStack(spacing: 14) {
                        Button { selection = .dataLab } label: {
                            Label("Generate a learning dataset", systemImage: "sparkles.rectangle.stack")
                                .frame(maxWidth: .infinity, minHeight: 42)
                        }.buttonStyle(.borderedProminent)
                        Button { selection = .prepare } label: {
                            Label("Import and inspect CSV", systemImage: "square.and.arrow.down")
                                .frame(maxWidth: .infinity, minHeight: 42)
                        }.buttonStyle(.bordered)
                    }
                }
                HelpNote(title: "What this tool can—and cannot—do",
                         text: "A model learns relationships that exist in representative data. Synthetic patterns are excellent for learning and testing, but they are not evidence that a model will work on real-world data. Always keep a held-out test set and validate the result where it will be used.")
            }.padding(30).frame(maxWidth: 1200, alignment: .leading)
        }
    }
    private func workflowCard(_ number: String, _ title: String, _ description: String,
                              _ symbol: String, _ color: Color, _ page: WorkflowPage) -> some View {
        Button { selection = page } label: {
            VStack(alignment: .leading, spacing: 14) {
                HStack {
                    Text(number).font(.caption.bold()).frame(width: 24, height: 24).background(color.opacity(0.2), in: Circle())
                    Spacer(); Image(systemName: symbol).font(.title2).foregroundStyle(color)
                }
                Text(title).font(.headline)
                Text(description).font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.leading)
                Spacer(minLength: 0)
                Label("Open", systemImage: "arrow.right").font(.caption.bold()).foregroundStyle(color)
            }
            .padding(17).frame(maxWidth: .infinity, minHeight: 190, alignment: .leading)
            .background(Color(nsColor: .controlBackgroundColor), in: RoundedRectangle(cornerRadius: 16))
        }.buttonStyle(.plain)
    }
}

