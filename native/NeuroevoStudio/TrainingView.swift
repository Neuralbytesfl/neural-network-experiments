import Charts
import SwiftUI

struct TrainingView: View {
    @EnvironmentObject private var model: StudioModel
    @Binding var selection: WorkflowPage?
    @State private var showAdvanced = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                PageHeader(eyebrow: "Step 3 · Train model", title: "Evolution workspace",
                           subtitle: "Candidate networks compete on training data. Validation performance selects the winner; test data remains the final unbiased check.")
                HStack(alignment: .top, spacing: 18) {
                    controls.frame(width: 340)
                    VStack(spacing: 16) {
                        metricGrid
                        scoreChart
                        HStack(alignment: .top, spacing: 16) {
                            topologyPanel
                            splitPanel
                        }
                        logPanel
                    }
                }
                HelpNote(title: "Better validation—not merely better training",
                         text: "A widening gap between training and validation scores can signal overfitting. Prefer a smaller network that generalizes well, then confirm performance exactly once on the held-out test split.")
            }.padding(30).frame(maxWidth: 1320, alignment: .leading)
        }
    }

    private var controls: some View {
        StudioPanel("Training setup", subtitle: "Start with defaults, then tune only when the chart gives you a reason") {
            PathButton(title: model.dataPath.isEmpty ? "Choose dataset" : "Change dataset", icon: "tablecells", action: model.chooseDataset)
            FilePathRow(label: "Dataset", path: model.dataPath)
            Picker("Problem type", selection: $model.isRegression) {
                Text("Classification").tag(false); Text("Regression").tag(true)
            }
            Toggle("CSV has header", isOn: $model.hasHeader)
            NumberSetting(title: "Target columns", value: $model.targetColumns, range: 1...16)
            Divider()
            NumberSetting(title: "Population", value: $model.population, range: 10...20_000)
            NumberSetting(title: "Generations", value: $model.generations, range: 1...100_000)
            NumberSetting(title: "Elite survivors", value: $model.elite, range: 1...10_000)
            NumberSetting(title: "Initial hidden units", value: $model.initialHidden, range: 1...512)
            NumberSetting(title: "Early-stop patience", value: $model.patience, range: 0...100_000)
            DisclosureGroup("Advanced evolution", isExpanded: $showAdvanced) {
                VStack(spacing: 9) {
                    DoubleSetting(title: "Target score", value: $model.targetScore, range: -1...1)
                    DoubleSetting(title: "Weight mutation", value: $model.weightRate, range: 0...1)
                    DoubleSetting(title: "Mutation strength", value: $model.weightSigma, range: 0...100)
                    DoubleSetting(title: "Topology mutation", value: $model.topologyRate, range: 0...1)
                    DoubleSetting(title: "Complexity penalty", value: $model.complexityPenalty, range: 0...1)
                    NumberSetting(title: "Worker threads (0 = auto)", value: $model.threads, range: 0...256)
                    HStack { Text("Seed"); Spacer(); TextField("Seed", value: $model.seed, format: .number).frame(width: 100) }
                }.padding(.top, 8)
            }
            Divider()
            PathButton(title: model.outputPath.isEmpty ? "Choose model destination" : "Change model destination", icon: "externaldrive", action: model.chooseOutput)
            FilePathRow(label: "Winner model", path: model.outputPath)
            if model.isTraining {
                ProgressView(value: model.progress)
                Button(role: .destructive, action: model.stopTraining) {
                    Label("Stop safely", systemImage: "stop.circle").frame(maxWidth: .infinity)
                }
            } else {
                Button(action: model.startTraining) {
                    Label("Start evolution", systemImage: "play.fill").frame(maxWidth: .infinity, minHeight: 34)
                }.buttonStyle(.borderedProminent).disabled(model.dataPath.isEmpty || model.outputPath.isEmpty)
            }
            Text(model.status).font(.caption).foregroundStyle(.secondary)
        }
    }

    private var metricGrid: some View {
        HStack(spacing: 12) {
            MetricCard(title: "Generation", value: model.generation.formatted(), symbol: "arrow.triangle.2.circlepath")
            MetricCard(title: "Validation", value: score(model.validationScore), symbol: "checkmark.seal")
            MetricCard(title: "Test", value: model.testScore.map(score) ?? "—", symbol: "lock.shield", color: .cyan)
            MetricCard(title: "Parameters", value: model.parameterCount.formatted(), symbol: "point.3.filled.connected.trianglepath.dotted")
        }
    }

    private var scoreChart: some View {
        StudioPanel("Learning curve", subtitle: "Training and validation score by generation") {
            if model.points.isEmpty {
                ContentUnavailableView("No training run yet", systemImage: "chart.xyaxis.line",
                                       description: Text("Start evolution to watch performance change."))
                    .frame(minHeight: 250)
            } else {
                Chart(model.points) { point in
                    LineMark(x: .value("Generation", point.generation), y: .value("Score", point.train), series: .value("Series", "Training"))
                        .foregroundStyle(.indigo).interpolationMethod(.catmullRom)
                    LineMark(x: .value("Generation", point.generation), y: .value("Score", point.validation), series: .value("Series", "Validation"))
                        .foregroundStyle(.mint).interpolationMethod(.catmullRom)
                }.chartYScale(domain: 0...1).frame(minHeight: 270)
            }
        }
    }

    private var topologyPanel: some View {
        StudioPanel("Winning topology", subtitle: "Layer sizes discovered by evolution") {
            Text(model.topology).font(.title3.monospaced().bold()).textSelection(.enabled)
            Text("Parameters: \(model.parameterCount.formatted()) · Evaluations: \(model.evaluations.formatted()) · \(model.elapsed.formatted(.number.precision(.fractionLength(2)))) s")
                .font(.caption).foregroundStyle(.secondary)
        }
    }

    private var splitPanel: some View {
        StudioPanel("Dataset split", subtitle: "Automatic deterministic split from the selected seed") {
            if let data = model.dataset {
                HStack { split("Train", data.train, .indigo); split("Validate", data.validation, .mint); split("Test", data.test, .cyan) }
                Text("\(data.inputs) input features · \(data.outputs) output\(data.outputs == 1 ? "" : "s")" + (data.classes > 0 ? " · \(data.classes) classes" : ""))
                    .font(.caption).foregroundStyle(.secondary)
            } else { Text("Inspect a valid dataset to calculate the split.").foregroundStyle(.secondary) }
        }
    }

    private var logPanel: some View {
        StudioPanel("Run log", subtitle: "Recent reproducible events and errors") {
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 5) {
                    ForEach(Array(model.logs.enumerated()), id: \.offset) { _, line in Text(line).font(.caption.monospaced()) }
                }.frame(maxWidth: .infinity, alignment: .leading)
            }.frame(height: 110)
            HStack { Spacer(); Button("Continue to Use Model") { selection = .useModel }.buttonStyle(.borderedProminent).disabled(model.modelPath.isEmpty) }
        }
    }

    private func split(_ name: String, _ rows: Int, _ color: Color) -> some View {
        VStack(alignment: .leading) { Text(name).font(.caption).foregroundStyle(.secondary); Text(rows.formatted()).bold().foregroundStyle(color) }
            .frame(maxWidth: .infinity, alignment: .leading)
    }
    private func score(_ value: Double) -> String { value.formatted(.number.precision(.fractionLength(4))) }
}
