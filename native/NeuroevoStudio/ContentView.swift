import Charts
import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var model: StudioModel

    var body: some View {
        NavigationSplitView {
            configuration
                .navigationSplitViewColumnWidth(min: 300, ideal: 330, max: 380)
        } detail: {
            dashboard
        }
        .background(Color(nsColor: .windowBackgroundColor))
    }

    private var configuration: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("NEUROEVO").font(.caption.bold()).foregroundStyle(.secondary)
                    Text("Studio").font(.largeTitle.bold())
                    Text("Evolve. Compare. Understand.").foregroundStyle(.secondary)
                }

                section("DATA") {
                    pathButton(title: model.dataPath.isEmpty ? "Choose CSV…" : URL(fileURLWithPath: model.dataPath).lastPathComponent,
                               icon: "tablecells", action: model.chooseDataset)
                    Picker("Task", selection: $model.isRegression) {
                        Text("Classification").tag(false)
                        Text("Regression").tag(true)
                    }.pickerStyle(.segmented)
                    Toggle("CSV has header", isOn: $model.hasHeader)
                    Stepper("Target columns: \(model.targetColumns)", value: $model.targetColumns, in: 1...32)
                    Button("Refresh dataset", action: model.inspectDataset).disabled(model.dataPath.isEmpty)
                }

                section("EVOLUTION") {
                    numberField("Population", value: $model.population)
                    numberField("Generations", value: $model.generations)
                    numberField("Elite", value: $model.elite)
                    numberField("Initial hidden", value: $model.initialHidden)
                    numberField("Patience", value: $model.patience)
                    numberField("Threads (0 = auto)", value: $model.threads)
                    HStack { Text("Seed"); Spacer(); TextField("Seed", value: $model.seed, format: .number).frame(width: 110) }
                    decimalField("Target score", value: $model.targetScore)
                    DisclosureGroup("Advanced mutation") {
                        VStack(spacing: 10) {
                            decimalField("Weight rate", value: $model.weightRate)
                            decimalField("Weight sigma", value: $model.weightSigma)
                            decimalField("Topology rate", value: $model.topologyRate)
                            decimalField("Complexity", value: $model.complexityPenalty)
                        }.padding(.top, 8)
                    }
                }

                section("OUTPUT") {
                    pathButton(title: model.outputPath.isEmpty ? "Choose model destination…" : URL(fileURLWithPath: model.outputPath).lastPathComponent,
                               icon: "externaldrive", action: model.chooseOutput)
                }

                if model.isTraining {
                    Button(role: .destructive, action: model.stopTraining) {
                        Label("Stop safely", systemImage: "stop.fill").frame(maxWidth: .infinity)
                    }.buttonStyle(.borderedProminent)
                } else {
                    Button(action: model.startTraining) {
                        Label("Start evolution", systemImage: "play.fill").frame(maxWidth: .infinity)
                    }.buttonStyle(.borderedProminent)
                        .controlSize(.large)
                        .disabled(model.dataPath.isEmpty || model.outputPath.isEmpty)
                }

                Text(model.backend).font(.caption).foregroundStyle(.secondary)
            }
            .padding(22)
        }
    }

    private var dashboard: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                HStack {
                    VStack(alignment: .leading, spacing: 3) {
                        Text("Evolution dashboard").font(.title.bold())
                        Text(model.status).foregroundStyle(.secondary)
                    }
                    Spacer()
                    statusPill
                }

                ProgressView(value: model.progress)
                    .tint(.mint)

                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 12), count: 4), spacing: 12) {
                    metricCard("GENERATION", "\(model.generation)", "arrow.triangle.2.circlepath")
                    metricCard("VALIDATION", score(model.validationScore), "checkmark.seal")
                    metricCard("TEST", model.testScore.map(score) ?? "—", "lock.shield")
                    metricCard("PARAMETERS", model.parameterCount.formatted(), "point.3.connected.trianglepath.dotted")
                }

                HStack(alignment: .top, spacing: 14) {
                    chartPanel.frame(minWidth: 520, minHeight: 320)
                    VStack(spacing: 14) {
                        datasetPanel
                        topologyPanel
                    }.frame(width: 310)
                }

                HStack(alignment: .top, spacing: 14) {
                    predictionPanel
                    coreMLPanel
                }

                logPanel
            }
            .padding(24)
        }
    }

    private var statusPill: some View {
        HStack(spacing: 7) {
            Circle().fill(model.isTraining ? .mint : .secondary).frame(width: 8, height: 8)
            Text(model.isTraining ? "RUNNING" : "IDLE").font(.caption.bold())
        }
        .padding(.horizontal, 12).padding(.vertical, 7)
        .background(.thinMaterial, in: Capsule())
    }

    private var chartPanel: some View {
        panel("Performance", subtitle: "Training and validation across generations") {
            if model.points.isEmpty {
                ContentUnavailableView("No generations yet", systemImage: "chart.xyaxis.line",
                                       description: Text("Start evolution to stream metrics."))
            } else {
                Chart(model.points) { point in
                    LineMark(x: .value("Generation", point.generation),
                             y: .value("Training", point.train))
                        .foregroundStyle(by: .value("Metric", "Training"))
                    LineMark(x: .value("Generation", point.generation),
                             y: .value("Validation", point.validation))
                        .foregroundStyle(by: .value("Metric", "Validation"))
                        .lineStyle(StrokeStyle(lineWidth: 2.5))
                }
                .chartForegroundStyleScale(["Training": Color.indigo, "Validation": Color.mint])
                .chartLegend(position: .top, alignment: .leading)
                .chartXAxisLabel("Generation")
                .chartYAxisLabel(model.isRegression ? "Score (−MSE)" : "Accuracy")
                .padding(.top, 8)
            }
        }
    }

    private var datasetPanel: some View {
        panel("Dataset", subtitle: model.dataPath.isEmpty ? "No file selected" : URL(fileURLWithPath: model.dataPath).lastPathComponent) {
            if let data = model.dataset {
                HStack {
                    smallMetric("INPUTS", data.inputs)
                    smallMetric("OUTPUTS", data.outputs)
                    smallMetric("CLASSES", data.classes)
                }
                Divider()
                splitRow("Train", data.train, color: .indigo)
                splitRow("Validation", data.validation, color: .mint)
                splitRow("Test", data.test, color: .orange)
            } else {
                Text("Choose a CSV to inspect its shape and split.").foregroundStyle(.secondary)
            }
        }
    }

    private var topologyPanel: some View {
        panel("Best topology", subtitle: "Current validation champion") {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 8) {
                    let nodes = model.topology.components(separatedBy: " → ")
                    ForEach(Array(nodes.enumerated()), id: \.offset) { index, node in
                        Text(node).font(.headline.monospacedDigit())
                            .frame(minWidth: 44, minHeight: 44)
                            .background(index == 0 ? Color.indigo.opacity(0.18) : Color.mint.opacity(0.18),
                                        in: RoundedRectangle(cornerRadius: 11))
                        if index + 1 < nodes.count { Image(systemName: "arrow.right").foregroundStyle(.secondary) }
                    }
                }
            }
            Text("\(model.evaluations.formatted()) evaluations · \(String(format: "%.2f", model.elapsed))s")
                .font(.caption).foregroundStyle(.secondary)
        }
    }

    private var predictionPanel: some View {
        panel("Native C++ prediction", subtitle: "Run the saved winner") {
            HStack {
                TextField("Comma-separated features", text: $model.predictionInput)
                    .textFieldStyle(.roundedBorder)
                Button("Predict", action: model.predict)
                Button("Choose model", action: model.chooseModel)
            }
            Text(model.predictionOutput).font(.system(.body, design: .monospaced)).textSelection(.enabled)
        }
    }

    private var coreMLPanel: some View {
        panel("Core ML deployment", subtitle: "CPU + Neural Engine allowed") {
            HStack {
                Button("Export winner", action: model.exportCoreML)
                Button("Run Core ML", action: model.runCoreMLPrediction).disabled(model.coreMLPath.isEmpty)
            }
            Text(model.coreMLStatus).font(.caption).foregroundStyle(.secondary).textSelection(.enabled)
        }
    }

    private var logPanel: some View {
        panel("Run log", subtitle: "Latest engine and application events") {
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 5) {
                    ForEach(Array(model.logs.enumerated()), id: \.offset) { _, line in
                        Text(line).font(.system(.caption, design: .monospaced)).frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
            }.frame(height: 140)
        }
    }

    private func section<Content: View>(_ title: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 11) {
            Text(title).font(.caption.bold()).foregroundStyle(.secondary)
            content()
        }
    }

    private func panel<Content: View>(_ title: String, subtitle: String,
                                      @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.headline)
                Text(subtitle).font(.caption).foregroundStyle(.secondary)
            }
            content()
        }
        .padding(17)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color(nsColor: .controlBackgroundColor), in: RoundedRectangle(cornerRadius: 16))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(.separator.opacity(0.4)))
    }

    private func metricCard(_ title: String, _ value: String, _ icon: String) -> some View {
        HStack {
            VStack(alignment: .leading, spacing: 7) {
                Text(title).font(.caption.bold()).foregroundStyle(.secondary)
                Text(value).font(.title2.bold().monospacedDigit())
                    .lineLimit(1).minimumScaleFactor(0.55)
            }
            Spacer()
            Image(systemName: icon).font(.title2).foregroundStyle(.mint)
        }
        .padding(16)
        .background(Color(nsColor: .controlBackgroundColor), in: RoundedRectangle(cornerRadius: 15))
    }

    private func pathButton(title: String, icon: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Label(title, systemImage: icon).lineLimit(1).frame(maxWidth: .infinity, alignment: .leading)
        }.buttonStyle(.bordered)
    }

    private func numberField(_ label: String, value: Binding<Int>) -> some View {
        HStack { Text(label); Spacer(); TextField(label, value: value, format: .number).frame(width: 90) }
    }

    private func decimalField(_ label: String, value: Binding<Double>) -> some View {
        HStack { Text(label); Spacer(); TextField(label, value: value, format: .number).frame(width: 90) }
    }

    private func smallMetric(_ label: String, _ value: Int) -> some View {
        VStack(alignment: .leading) { Text(label).font(.caption2).foregroundStyle(.secondary); Text(value.formatted()).font(.headline) }
            .frame(maxWidth: .infinity, alignment: .leading)
    }

    private func splitRow(_ label: String, _ count: Int, color: Color) -> some View {
        HStack { Circle().fill(color).frame(width: 7, height: 7); Text(label); Spacer(); Text(count.formatted()).monospacedDigit() }
            .font(.subheadline)
    }

    private func score(_ value: Double) -> String { String(format: "%.5f", value) }
}
