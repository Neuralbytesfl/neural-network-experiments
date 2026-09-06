import AppKit
import CNeuroevo
import CoreML
import Foundation

private func decodedCString(_ bytes: [CChar]) -> String {
    String(decoding: bytes.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
}

struct MetricPoint: Identifiable, Sendable {
    let id = UUID()
    let generation: Int
    let train: Double
    let validation: Double
    let meanFitness: Double
}

struct DatasetSummary: Sendable {
    let inputs: Int
    let outputs: Int
    let classes: Int
    let train: Int
    let validation: Int
    let test: Int
}

struct ProgressSnapshot: Sendable {
    let generation: Int
    let limit: Int
    let train: Double
    let validation: Double
    let meanFitness: Double
    let parameters: Int
    let evaluations: Int
    let elapsed: Double
    let topology: String
}

struct TrainingSummary: Sendable {
    let cancelled: Bool
    let validation: Double
    let test: Double
    let testLoss: Double
    let parameters: Int
    let evaluations: Int
    let elapsed: Double
    let topology: String
}

struct TrainingSettings: Sendable {
    let dataPath: String
    let outputPath: String
    let isRegression: Bool
    let hasHeader: Bool
    let targetColumns: Int
    let population: Int
    let generations: Int
    let elite: Int
    let threads: Int
    let seed: UInt64
    let initialHidden: Int
    let patience: Int
    let targetScore: Double
    let weightRate: Double
    let weightSigma: Double
    let topologyRate: Double
    let complexityPenalty: Double
}

private enum WorkerResult: Sendable {
    case success(TrainingSummary)
    case failure(String)
}

private final class TrainingWorker: @unchecked Sendable {
    private let session: OpaquePointer?
    private let settings: TrainingSettings
    private let onProgress: @Sendable (ProgressSnapshot) -> Void
    private let onFinish: @Sendable (WorkerResult) -> Void

    init(settings: TrainingSettings,
         onProgress: @escaping @Sendable (ProgressSnapshot) -> Void,
         onFinish: @escaping @Sendable (WorkerResult) -> Void) {
        self.settings = settings
        self.onProgress = onProgress
        self.onFinish = onFinish
        session = ne_session_create()
    }

    deinit {
        ne_session_destroy(session)
    }

    func start() {
        DispatchQueue.global(qos: .userInitiated).async { [self] in
            onFinish(run())
        }
    }

    func cancel() {
        ne_session_cancel(session)
    }

    fileprivate func accept(_ progress: UnsafePointer<NEProgress>) {
        let value = progress.pointee
        onProgress(ProgressSnapshot(
            generation: Int(value.generation),
            limit: Int(value.generation_limit),
            train: value.best_train_score,
            validation: value.best_validation_score,
            meanFitness: value.mean_fitness,
            parameters: Int(value.parameter_count),
            evaluations: Int(value.evaluations),
            elapsed: value.elapsed_seconds,
            topology: value.topology.map { String(cString: $0) } ?? "—"
        ))
    }

    private func run() -> WorkerResult {
        guard let session else { return .failure("Unable to create C++ training session") }
        var result = NETrainResult()
        var error = [CChar](repeating: 0, count: 2048)
        let code: Int32 = settings.dataPath.withCString { data in
            settings.outputPath.withCString { output in
                var config = makeConfig(data: data, output: output)
                return ne_train(session, &config, studioProgressCallback,
                                Unmanaged.passUnretained(self).toOpaque(),
                                &result, &error, error.count)
            }
        }
        if code != 0 { return .failure(decodedCString(error)) }
        let topology = withUnsafePointer(to: &result.topology) {
            $0.withMemoryRebound(to: CChar.self, capacity: 256) { String(cString: $0) }
        }
        return .success(TrainingSummary(
            cancelled: result.cancelled != 0,
            validation: result.validation_score,
            test: result.test_score,
            testLoss: result.test_loss,
            parameters: Int(result.parameter_count),
            evaluations: Int(result.evaluations),
            elapsed: result.elapsed_seconds,
            topology: topology
        ))
    }

    private func makeConfig(data: UnsafePointer<CChar>,
                            output: UnsafePointer<CChar>) -> NEConfig {
        var config = NEConfig()
        ne_default_config(&config)
        config.data_path = data
        config.output_path = output
        config.task = settings.isRegression ? NE_TASK_REGRESSION : NE_TASK_CLASSIFICATION
        config.target_columns = settings.targetColumns
        config.has_header = settings.hasHeader ? 1 : 0
        config.population_size = settings.population
        config.generations = settings.generations
        config.elite_count = settings.elite
        config.threads = settings.threads
        config.seed = settings.seed
        config.initial_hidden = settings.initialHidden
        config.patience = settings.patience
        config.target_score = settings.targetScore
        config.weight_mutation_rate = settings.weightRate
        config.weight_mutation_sigma = settings.weightSigma
        config.topology_mutation_rate = settings.topologyRate
        config.complexity_penalty = settings.complexityPenalty
        return config
    }
}

private let studioProgressCallback: NEProgressCallback = { progress, context in
    guard let progress, let context else { return }
    Unmanaged<TrainingWorker>.fromOpaque(context).takeUnretainedValue().accept(progress)
}

@MainActor
final class StudioModel: ObservableObject {
    @Published var dataPath = ""
    @Published var outputPath = ""
    @Published var modelPath = ""
    @Published var coreMLPath = ""
    @Published var predictionInput = "0,1"
    @Published var predictionOutput = "—"
    @Published var coreMLStatus = "Not exported"
    @Published var isRegression = false
    @Published var hasHeader = true
    @Published var targetColumns = 1
    @Published var population = 160
    @Published var generations = 150
    @Published var elite = 8
    @Published var threads = 0
    @Published var seed: UInt64 = 42
    @Published var initialHidden = 6
    @Published var patience = 40
    @Published var targetScore = 0.995
    @Published var weightRate = 0.12
    @Published var weightSigma = 0.35
    @Published var topologyRate = 0.12
    @Published var complexityPenalty = 0.000001

    @Published private(set) var dataset: DatasetSummary?
    @Published private(set) var points: [MetricPoint] = []
    @Published private(set) var isTraining = false
    @Published private(set) var progress = 0.0
    @Published private(set) var generation = 0
    @Published private(set) var trainScore = 0.0
    @Published private(set) var validationScore = 0.0
    @Published private(set) var testScore: Double?
    @Published private(set) var meanFitness = 0.0
    @Published private(set) var parameterCount = 0
    @Published private(set) var evaluations = 0
    @Published private(set) var elapsed = 0.0
    @Published private(set) var topology = "—"
    @Published private(set) var status = "Choose a numeric CSV dataset"
    @Published private(set) var logs: [String] = []

    private var worker: TrainingWorker?

    init() {
        let arguments = CommandLine.arguments
        if let index = arguments.firstIndex(of: "--data"), index + 1 < arguments.count {
            dataPath = arguments[index + 1]
        }
        if let index = arguments.firstIndex(of: "--output"), index + 1 < arguments.count {
            outputPath = arguments[index + 1]
        }
        if !dataPath.isEmpty {
            if outputPath.isEmpty {
                outputPath = URL(fileURLWithPath: dataPath)
                    .deletingPathExtension().appendingPathExtension("neuroevo").path
            }
            inspectDataset()
        }
    }

    var backend: String {
        ne_backend_name().map { String(cString: $0) } ?? "Unknown backend"
    }

    func chooseDataset() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.commaSeparatedText, .plainText]
        panel.allowsMultipleSelection = false
        if panel.runModal() == .OK, let url = panel.url {
            dataPath = url.path
            if outputPath.isEmpty {
                outputPath = url.deletingPathExtension().appendingPathExtension("neuroevo").path
            }
            inspectDataset()
        }
    }

    func chooseOutput() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.data]
        panel.nameFieldStringValue = URL(fileURLWithPath: outputPath).lastPathComponent.isEmpty
            ? "winner.neuroevo" : URL(fileURLWithPath: outputPath).lastPathComponent
        if panel.runModal() == .OK, let url = panel.url { outputPath = url.path }
    }

    func chooseModel() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.data]
        if panel.runModal() == .OK, let url = panel.url { modelPath = url.path }
    }

    func inspectDataset() {
        guard !dataPath.isEmpty else { return }
        var info = NEDatasetInfo()
        var error = [CChar](repeating: 0, count: 1024)
        let code: Int32 = dataPath.withCString { data in
            var config = NEConfig()
            ne_default_config(&config)
            config.data_path = data
            config.task = isRegression ? NE_TASK_REGRESSION : NE_TASK_CLASSIFICATION
            config.target_columns = targetColumns
            config.has_header = hasHeader ? 1 : 0
            config.seed = seed
            return ne_inspect_dataset(&config, &info, &error, error.count)
        }
        if code == 0 {
            dataset = DatasetSummary(inputs: Int(info.input_count), outputs: Int(info.output_count),
                                     classes: Int(info.class_count), train: Int(info.train_rows),
                                     validation: Int(info.validation_rows), test: Int(info.test_rows))
            status = "Dataset ready"
            appendLog("Loaded \(URL(fileURLWithPath: dataPath).lastPathComponent): \(info.input_count) inputs, \(info.output_count) outputs")
        } else {
            dataset = nil
            status = decodedCString(error)
            appendLog("Dataset error: \(status)")
        }
    }

    func startTraining() {
        guard !dataPath.isEmpty, !outputPath.isEmpty else {
            status = "Choose both dataset and output model paths"
            return
        }
        points.removeAll()
        testScore = nil
        progress = 0
        isTraining = true
        status = "Evolution running"
        appendLog("Started population \(population), generations \(generations), seed \(seed)")
        let settings = TrainingSettings(
            dataPath: dataPath, outputPath: outputPath, isRegression: isRegression,
            hasHeader: hasHeader, targetColumns: targetColumns, population: population,
            generations: generations, elite: elite, threads: threads, seed: seed,
            initialHidden: initialHidden, patience: patience, targetScore: targetScore,
            weightRate: weightRate, weightSigma: weightSigma,
            topologyRate: topologyRate, complexityPenalty: complexityPenalty
        )
        let newWorker = TrainingWorker(settings: settings, onProgress: { [weak self] snapshot in
            Task { @MainActor in self?.apply(snapshot) }
        }, onFinish: { [weak self] result in
            Task { @MainActor in self?.finish(result) }
        })
        worker = newWorker
        newWorker.start()
    }

    func stopTraining() {
        status = "Stopping after the current generation…"
        worker?.cancel()
    }

    func predict() {
        let path = modelPath.isEmpty ? outputPath : modelPath
        guard !path.isEmpty else { predictionOutput = "Choose or train a model"; return }
        do {
            let features = try parseFeatures(predictionInput)
            var prediction = NEPrediction()
            var error = [CChar](repeating: 0, count: 1024)
            let code: Int32 = path.withCString { model in
                features.withUnsafeBufferPointer { buffer in
                    ne_predict(model, buffer.baseAddress, buffer.count,
                               &prediction, &error, error.count)
                }
            }
            guard code == 0 else { predictionOutput = decodedCString(error); return }
            let values = (0..<Int(prediction.count)).map {
                String(format: "%.6g", ne_prediction_value(&prediction, $0))
            }.joined(separator: ", ")
            predictionOutput = prediction.is_classification != 0
                ? "Class \(String(format: "%.6g", prediction.class_value)) · [\(values)]"
                : "[\(values)]"
        } catch {
            predictionOutput = error.localizedDescription
        }
    }

    func exportCoreML() {
        let source = modelPath.isEmpty ? outputPath : modelPath
        guard !source.isEmpty else { coreMLStatus = "Choose or train a model first"; return }
        let panel = NSSavePanel()
        panel.nameFieldStringValue = URL(fileURLWithPath: source).deletingPathExtension()
            .appendingPathExtension("mlmodel").lastPathComponent
        if panel.runModal() != .OK || panel.url == nil { return }
        let destination = panel.url!.path
        coreMLStatus = "Exporting…"
        let script = exporterScriptPath()
        let python = coreMLPython(for: source)
        DispatchQueue.global(qos: .userInitiated).async { [source] in
            let process = Process()
            let pipe = Pipe()
            process.executableURL = URL(fileURLWithPath: python)
            process.arguments = [script, "--model", source, "--output", destination]
            process.standardOutput = pipe
            process.standardError = pipe
            do {
                try process.run()
                process.waitUntilExit()
                let data = pipe.fileHandleForReading.readDataToEndOfFile()
                let message = String(data: data, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
                Task { @MainActor [weak self] in
                    if process.terminationStatus == 0 {
                        self?.coreMLPath = destination
                        self?.coreMLStatus = "Exported · CPU + Neural Engine eligible"
                        self?.appendLog(message)
                    } else {
                        self?.coreMLStatus = message.isEmpty ? "Core ML export failed" : message
                    }
                }
            } catch {
                Task { @MainActor [weak self] in self?.coreMLStatus = error.localizedDescription }
            }
        }
    }

    func runCoreMLPrediction() {
        guard !coreMLPath.isEmpty else { coreMLStatus = "Export or select a Core ML model"; return }
        do {
            let values = try parseFeatures(predictionInput)
            let configuration = MLModelConfiguration()
            configuration.computeUnits = .cpuAndNeuralEngine
            let compiled = try MLModel.compileModel(at: URL(fileURLWithPath: coreMLPath))
            let model = try MLModel(contentsOf: compiled, configuration: configuration)
            let array = try MLMultiArray(shape: [NSNumber(value: values.count)], dataType: .float32)
            for (index, value) in values.enumerated() { array[index] = NSNumber(value: value) }
            let provider = try MLDictionaryFeatureProvider(dictionary: ["features": array])
            let output = try model.prediction(from: provider)
            guard let prediction = output.featureValue(for: "prediction")?.multiArrayValue else {
                throw NSError(domain: "NeuroevoStudio", code: 1,
                              userInfo: [NSLocalizedDescriptionKey: "Core ML output 'prediction' is missing"])
            }
            let rendered = (0..<prediction.count).map { String(format: "%.6g", prediction[$0].doubleValue) }
            coreMLStatus = "CPU + Neural Engine allowed · output [\(rendered.joined(separator: ", "))]"
        } catch {
            coreMLStatus = error.localizedDescription
        }
    }

    private func apply(_ snapshot: ProgressSnapshot) {
        generation = snapshot.generation
        trainScore = snapshot.train
        validationScore = snapshot.validation
        meanFitness = snapshot.meanFitness
        parameterCount = snapshot.parameters
        evaluations = snapshot.evaluations
        elapsed = snapshot.elapsed
        topology = snapshot.topology
        progress = min(1, Double(snapshot.generation + 1) / Double(max(1, snapshot.limit)))
        points.append(MetricPoint(generation: snapshot.generation, train: snapshot.train,
                                  validation: snapshot.validation, meanFitness: snapshot.meanFitness))
        if snapshot.generation == 0 || snapshot.generation % 5 == 0 {
            appendLog("Gen \(snapshot.generation): validation \(format(snapshot.validation)), \(snapshot.parameters) params")
        }
    }

    private func finish(_ result: WorkerResult) {
        isTraining = false
        worker = nil
        switch result {
        case .failure(let message):
            status = message
            appendLog("Training failed: \(message)")
        case .success(let summary):
            validationScore = summary.validation
            testScore = summary.test
            parameterCount = summary.parameters
            evaluations = summary.evaluations
            elapsed = summary.elapsed
            topology = summary.topology
            modelPath = outputPath
            status = summary.cancelled ? "Stopped safely; best-so-far model saved" : "Training complete"
            appendLog("\(status). Test \(format(summary.test)), loss \(format(summary.testLoss))")
        }
    }

    private func appendLog(_ line: String) {
        logs.append("\(Date.now.formatted(date: .omitted, time: .standard))  \(line)")
        if logs.count > 300 { logs.removeFirst(logs.count - 300) }
    }

    private func parseFeatures(_ text: String) throws -> [Float] {
        let pieces = text.split(separator: ",", omittingEmptySubsequences: false)
        let values = pieces.compactMap { Float($0.trimmingCharacters(in: .whitespaces)) }
        guard !values.isEmpty, values.count == pieces.count else {
            throw NSError(domain: "NeuroevoStudio", code: 2,
                          userInfo: [NSLocalizedDescriptionKey: "Enter comma-separated numeric features"])
        }
        return values
    }

    private func exporterScriptPath() -> String {
        if let bundled = Bundle.main.path(forResource: "export_coreml", ofType: "py") { return bundled }
        return URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
            .appendingPathComponent("tools/export_coreml.py").path
    }

    private func coreMLPython(for model: String) -> String {
        let manager = FileManager.default
        let currentCandidate = URL(fileURLWithPath: manager.currentDirectoryPath)
            .appendingPathComponent(".venv-coreml/bin/python").path
        let modelCandidate = URL(fileURLWithPath: model).deletingLastPathComponent()
            .deletingLastPathComponent().appendingPathComponent(".venv-coreml/bin/python").path
        return [currentCandidate, modelCandidate, "/usr/bin/python3"]
            .first(where: { manager.isExecutableFile(atPath: $0) }) ?? "/usr/bin/python3"
    }

    private func format(_ value: Double) -> String { String(format: "%.5f", value) }
}
