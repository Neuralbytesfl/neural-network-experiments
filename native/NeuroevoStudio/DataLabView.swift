import Charts
import SwiftUI

struct DataLabView: View {
    @EnvironmentObject private var model: StudioModel
    @Binding var selection: WorkflowPage?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                PageHeader(eyebrow: "Step 1 · Create data", title: "Data Lab",
                           subtitle: "Generate a reproducible pattern to learn with, or import your own numeric CSV.")
                HStack(alignment: .top, spacing: 18) {
                    StudioPanel("Synthetic generator", subtitle: "Every run is reproducible from its seed") {
                        Picker("Pattern", selection: $model.syntheticPattern) {
                            ForEach(SyntheticPattern.allCases) { pattern in
                                Label(pattern.rawValue, systemImage: pattern.symbol).tag(pattern)
                            }
                        }
                        .pickerStyle(.menu)
                        NumberSetting(title: "Rows", value: $model.generatedRows, range: 20...1_000_000)
                        if !model.syntheticPattern.classification {
                            NumberSetting(title: "Input features", value: $model.generatedInputs, range: 1...16)
                        }
                        DoubleSetting(title: "Minimum", value: $model.generatedMinimum, range: -1_000...1_000)
                        DoubleSetting(title: "Maximum", value: $model.generatedMaximum, range: -1_000...1_000)
                        DoubleSetting(title: model.syntheticPattern.classification ? "Noise (probability/spread)" : "Target noise",
                                      value: $model.generatedNoise,
                                      range: 0...(model.syntheticPattern.classification ? 0.5 : 100))
                        HStack {
                            Text("Random seed"); Spacer()
                            TextField("Seed", value: $model.generatedSeed, format: .number).frame(width: 100)
                        }
                        Divider()
                        PathButton(title: model.generatedPath.isEmpty ? "Choose CSV destination" : "Change destination",
                                   icon: "folder", action: model.chooseGeneratedOutput)
                        FilePathRow(label: "Output", path: model.generatedPath)
                        Button(action: { model.generateSyntheticData() }) {
                            Label("Generate dataset", systemImage: "sparkles")
                                .frame(maxWidth: .infinity, minHeight: 32)
                        }.buttonStyle(.borderedProminent)
                        Text(model.dataLabStatus).font(.caption).foregroundStyle(.secondary)
                    }.frame(width: 330)

                    VStack(spacing: 18) {
                        StudioPanel("Pattern preview", subtitle: model.generatedFormula) {
                            if model.dataPreview.isEmpty {
                                ContentUnavailableView("No preview yet", systemImage: "chart.dots.scatter",
                                                       description: Text("Generate or import data to see up to 800 rows."))
                                    .frame(minHeight: 300)
                            } else {
                                Chart(model.dataPreview) { point in
                                    PointMark(x: .value("Feature 1", point.x), y: .value("Feature 2", point.y))
                                        .foregroundStyle(by: .value("Target", roundedTarget(point.target)))
                                        .symbolSize(22)
                                }
                                .chartLegend(position: .bottom)
                                .frame(minHeight: 330)
                            }
                        }
                        StudioPanel("Bring your own data", subtitle: "Numeric CSV with target column(s) at the end") {
                            HStack {
                                PathButton(title: "Choose CSV", icon: "square.and.arrow.down", action: model.chooseDataset)
                                Button("Continue to Prepare") { selection = .prepare }.buttonStyle(.borderedProminent)
                            }
                            FilePathRow(label: "Current dataset", path: model.dataPath)
                        }
                    }
                }
                HelpNote(title: "Synthetic data is a controlled experiment",
                         text: "Use it to understand learning behavior, verify a pipeline, or compare settings. For a real decision, train and test on representative real-world data collected for that purpose.")
            }.padding(30).frame(maxWidth: 1250, alignment: .leading)
        }
    }

    private func roundedTarget(_ value: Double) -> String {
        model.isRegression ? String(format: "%.2f", value) : "Class \(Int(value.rounded()))"
    }
}
