import SwiftUI

struct PrepareDataView: View {
    @EnvironmentObject private var model: StudioModel
    @Binding var selection: WorkflowPage?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                PageHeader(eyebrow: "Step 2 · Prepare data", title: "Inspect before you train",
                           subtitle: "Measure data quality and create a cleaned copy with an audit summary. Your source file is never overwritten.")
                StudioPanel("Source dataset", subtitle: "Describe the CSV so labels and features are interpreted correctly") {
                    HStack(spacing: 12) {
                        PathButton(title: "Choose CSV", icon: "folder", action: model.chooseDataset)
                        Picker("Task", selection: $model.isRegression) {
                            Text("Classification").tag(false); Text("Regression").tag(true)
                        }.frame(width: 210)
                        Toggle("Header row", isOn: $model.hasHeader)
                        Stepper("Target columns: \(model.targetColumns)", value: $model.targetColumns, in: 1...16)
                        Button("Refresh") { model.profileCurrentData(); model.inspectDataset() }
                    }
                    FilePathRow(label: "Input", path: model.dataPath)
                }
                qualityGrid
                HStack(alignment: .top, spacing: 18) {
                    StudioPanel("Cleaning recipe", subtitle: "Conservative operations with an explicit output file") {
                        Toggle("Fill missing feature values with column means", isOn: $model.imputeMissing)
                        Toggle("Remove exact duplicate rows", isOn: $model.removeDuplicates)
                        Toggle("Drop rows with the wrong column count", isOn: $model.dropMalformed)
                        Toggle("Clip extreme feature values", isOn: $model.clipOutliers)
                        if model.clipOutliers {
                            DoubleSetting(title: "Z-score limit", value: $model.clipZScore, range: 1...20)
                        }
                        Divider()
                        PathButton(title: model.cleanedPath.isEmpty ? "Choose cleaned CSV destination" : "Change destination",
                                   icon: "folder.badge.plus", action: model.chooseCleanedOutput)
                        FilePathRow(label: "Cleaned copy", path: model.cleanedPath)
                        Button(action: model.cleanCurrentData) {
                            Label("Create cleaned copy", systemImage: "wand.and.stars")
                                .frame(maxWidth: .infinity, minHeight: 32)
                        }.buttonStyle(.borderedProminent).disabled(model.dataPath.isEmpty)
                    }
                    StudioPanel("Cleaning audit", subtitle: "A factual record of what changed") {
                        if let summary = model.cleaningSummary {
                            auditRow("Rows read", summary.rowsRead)
                            auditRow("Rows retained", summary.rowsWritten)
                            auditRow("Rows dropped", summary.rowsDropped)
                            auditRow("Missing values filled", summary.imputed)
                            auditRow("Duplicates removed", summary.duplicatesRemoved)
                            auditRow("Extreme values clipped", summary.valuesClipped)
                        } else {
                            ContentUnavailableView("No cleaning run yet", systemImage: "checklist",
                                                   description: Text("Choose a source and create a cleaned copy."))
                        }
                        Text(model.cleaningStatus).font(.caption).foregroundStyle(.secondary)
                        Button("Continue to Train") { selection = .train }
                            .buttonStyle(.borderedProminent).disabled(model.dataPath.isEmpty)
                    }
                }
                HelpNote(title: "Prevent data leakage",
                         text: "In a production workflow, learn imputation and clipping values from the training split only, then apply those same values to validation, test, and live data. This first release cleans before splitting, so use it for exploration and non-sensitive work while validating the complete pipeline independently.")
            }.padding(30).frame(maxWidth: 1250, alignment: .leading)
        }
    }

    @ViewBuilder private var qualityGrid: some View {
        if let profile = model.qualityProfile {
            HStack(spacing: 12) {
                MetricCard(title: "Quality score", value: profile.qualityScore.formatted(.percent.precision(.fractionLength(0))), symbol: "gauge.with.dots.needle.50percent")
                MetricCard(title: "Rows", value: profile.rows.formatted(), symbol: "list.number")
                MetricCard(title: "Missing cells", value: profile.missingCells.formatted(), symbol: "questionmark.square", color: profile.missingCells == 0 ? .mint : .orange)
                MetricCard(title: "Malformed", value: profile.malformedRows.formatted(), symbol: "exclamationmark.triangle", color: profile.malformedRows == 0 ? .mint : .orange)
                MetricCard(title: "Duplicates", value: profile.duplicateRows.formatted(), symbol: "square.on.square", color: profile.duplicateRows == 0 ? .mint : .orange)
            }
        } else {
            HelpNote(title: "No profile yet", text: "Choose or generate a dataset to calculate structural quality metrics.")
        }
    }

    private func auditRow(_ name: String, _ value: Int) -> some View {
        HStack { Text(name); Spacer(); Text(value.formatted()).monospacedDigit().bold() }
    }
}
