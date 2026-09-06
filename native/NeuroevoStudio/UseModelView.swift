import SwiftUI

struct UseModelView: View {
    @EnvironmentObject private var model: StudioModel
    @Binding var selection: WorkflowPage?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                PageHeader(eyebrow: "Step 4 · Use model", title: "Turn the winner into an answer",
                           subtitle: "Test individual inputs with the native model, then export to Core ML for integration into an Apple application.")
                HStack(alignment: .top, spacing: 18) {
                    StudioPanel("Native prediction", subtitle: "Run the saved C++ model directly") {
                        HStack {
                            PathButton(title: "Choose .neuroevo model", icon: "doc", action: model.chooseModel)
                            if !model.outputPath.isEmpty { Button("Use latest winner") { model.modelPath = model.outputPath } }
                        }
                        FilePathRow(label: "Model", path: model.modelPath.isEmpty ? model.outputPath : model.modelPath)
                        Text("Comma-separated feature values").font(.caption.bold())
                        TextField("Example: 0.25, -0.6", text: $model.predictionInput).textFieldStyle(.roundedBorder)
                        Button(action: model.predict) {
                            Label("Predict", systemImage: "bolt.fill").frame(maxWidth: .infinity, minHeight: 34)
                        }.buttonStyle(.borderedProminent)
                        GroupBox("Result") {
                            Text(model.predictionOutput).font(.title3.monospaced()).textSelection(.enabled)
                                .frame(maxWidth: .infinity, minHeight: 55, alignment: .leading)
                        }
                    }
                    StudioPanel("Core ML export", subtitle: "Create an Apple-native model artifact") {
                        Label("Core ML can schedule supported operations across CPU and Apple Neural Engine.", systemImage: "apple.logo")
                            .font(.subheadline)
                        Button(action: model.exportCoreML) {
                            Label("Export .mlmodel", systemImage: "square.and.arrow.up").frame(maxWidth: .infinity)
                        }.buttonStyle(.borderedProminent)
                        FilePathRow(label: "Core ML model", path: model.coreMLPath)
                        Button("Test with CPU + Neural Engine allowed", action: model.runCoreMLPrediction)
                            .buttonStyle(.bordered).disabled(model.coreMLPath.isEmpty)
                        Text(model.coreMLStatus).font(.caption).foregroundStyle(.secondary).textSelection(.enabled)
                        Divider()
                        Text("Neural Engine eligibility is not a guarantee of where every operation runs. Core ML chooses the available compute unit based on model operations and the device.")
                            .font(.caption).foregroundStyle(.secondary)
                    }
                }
                StudioPanel("Production checklist", subtitle: "A model file is only one part of a reliable product") {
                    HStack(alignment: .top, spacing: 28) {
                        checklist("Data contract", ["Preserve feature order", "Apply identical preprocessing", "Reject missing or invalid inputs"])
                        checklist("Validation", ["Test unseen real examples", "Measure failures by subgroup", "Set a safe confidence policy"])
                        checklist("Operations", ["Version model and dataset", "Monitor drift and errors", "Keep a rollback model"])
                    }
                    HStack { Spacer(); Button("Open the learning manual") { selection = .learn }.buttonStyle(.borderedProminent) }
                }
                HelpNote(title: "Predictions are estimates",
                         text: "Do not use this experimental tool as the sole basis for medical, legal, financial, safety-critical, or other high-impact decisions. Validate performance, bias, privacy, and failure handling for the actual use case.")
            }.padding(30).frame(maxWidth: 1200, alignment: .leading)
        }
    }

    private func checklist(_ title: String, _ items: [String]) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title).font(.headline)
            ForEach(items, id: \.self) { Label($0, systemImage: "checkmark.circle").font(.subheadline) }
        }.frame(maxWidth: .infinity, alignment: .leading)
    }
}
