import Foundation

private enum ControlFailure: LocalizedError {
    case message(String)
    var errorDescription: String? {
        switch self { case .message(let value): return value }
    }
}

@MainActor
extension StudioModel {
    func prepareControlDirectory() {
        let path = "/tmp/neuroevo-studio-control-\(getuid())"
        do {
            try FileManager.default.createDirectory(atPath: path, withIntermediateDirectories: true)
            try FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: path)
            controlDirectoryPath = path
            let ready: [String: Any] = [
                "protocol": 1,
                "pid": ProcessInfo.processInfo.processIdentifier,
                "app": "Neuroevo Studio"
            ]
            try writeControlJSON(ready, to: URL(fileURLWithPath: path).appendingPathComponent("ready.json"))
            for url in try FileManager.default.contentsOfDirectory(at: URL(fileURLWithPath: path),
                                                                    includingPropertiesForKeys: nil)
            where url.lastPathComponent.hasPrefix("command-") {
                try? FileManager.default.removeItem(at: url)
            }
        } catch {
            controlDirectoryPath = ""
            appendLog("CLI control unavailable: \(error.localizedDescription)")
        }
    }

    func startControlLoop() {
        guard controlTask == nil else { return }
        // A detached poller is owned by the model rather than SwiftUI view
        // identity, so sidebar navigation cannot cancel CLI control.
        controlTask = Task.detached { [weak self] in
            while !Task.isCancelled {
                await self?.processControlCommands()
                try? await Task.sleep(for: .milliseconds(100))
            }
        }
    }

    private func processControlCommands() {
        guard !controlDirectoryPath.isEmpty else { return }
        let directory = URL(fileURLWithPath: controlDirectoryPath)
        guard let files = try? FileManager.default.contentsOfDirectory(at: directory,
                                                                        includingPropertiesForKeys: nil)
            .filter({ $0.lastPathComponent.hasPrefix("command-") && $0.pathExtension == "json" })
            .sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) else { return }
        for file in files.prefix(20) {
            processControlCommand(at: file, directory: directory)
        }
    }

    private func processControlCommand(at file: URL, directory: URL) {
        var identifier = file.deletingPathExtension().lastPathComponent
            .replacingOccurrences(of: "command-", with: "")
        var response: [String: Any]
        do {
            let data = try Data(contentsOf: file)
            guard let request = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
                throw ControlFailure.message("request must be a JSON object")
            }
            identifier = request["id"] as? String ?? identifier
            guard let command = request["command"] as? String else {
                throw ControlFailure.message("request is missing command")
            }
            let arguments = request["arguments"] as? [String: Any] ?? [:]
            try handleControl(command: command, arguments: arguments)
            response = ["id": identifier, "ok": true, "state": controlState()]
        } catch {
            response = ["id": identifier, "ok": false,
                        "error": error.localizedDescription, "state": controlState()]
        }
        let reply = directory.appendingPathComponent("reply-\(identifier).json")
        try? writeControlJSON(response, to: reply)
        try? FileManager.default.removeItem(at: file)
    }

    private func handleControl(command: String, arguments: [String: Any]) throws {
        switch command.lowercased() {
        case "ping", "status": break
        case "page":
            guard let name = arguments["name"] as? String, let page = WorkflowPage.parse(name) else {
                throw ControlFailure.message("page must be welcome, create, prepare, train, use, or learn")
            }
            selectedPage = page
        case "set":
            guard let name = arguments["name"] as? String, let value = arguments["value"] else {
                throw ControlFailure.message("set requires name and value")
            }
            try setControlValue(name: name, value: value)
        case "load":
            dataPath = try requiredString(arguments, "path")
            if let task = arguments["task"] as? String { isRegression = task.lowercased() == "regression" }
            if let header = arguments["header"] { hasHeader = try boolean(header, name: "header") }
            if let targets = arguments["targets"] { targetColumns = try integer(targets, name: "targets") }
            outputPath = URL(fileURLWithPath: dataPath).deletingPathExtension()
                .appendingPathExtension("neuroevo").path
            inspectDataset(); profileCurrentData(); loadPreview(from: dataPath)
            guard dataset != nil else { throw ControlFailure.message(status) }
        case "generate":
            generatedPath = try requiredString(arguments, "output")
            if let patternName = arguments["pattern"] as? String {
                guard let pattern = pattern(named: patternName) else {
                    throw ControlFailure.message("unknown pattern: \(patternName)")
                }
                syntheticPattern = pattern
            }
            if let value = arguments["rows"] { generatedRows = try integer(value, name: "rows") }
            if let value = arguments["inputs"] { generatedInputs = try integer(value, name: "inputs") }
            if let value = arguments["noise"] { generatedNoise = try number(value, name: "noise") }
            if let value = arguments["seed"] { generatedSeed = try unsigned(value, name: "seed") }
            guard generateSyntheticData() else {
                throw ControlFailure.message(dataLabStatus)
            }
            selectedPage = .dataLab
        case "profile":
            profileCurrentData(); selectedPage = .prepare
            guard qualityProfile != nil else { throw ControlFailure.message(cleaningStatus) }
        case "clean":
            cleanedPath = try requiredString(arguments, "output")
            let cleaned = cleanCurrentData(); selectedPage = .prepare
            guard cleaned else {
                throw ControlFailure.message(cleaningStatus)
            }
        case "train":
            if let path = arguments["data"] as? String { dataPath = path }
            if let path = arguments["output"] as? String { outputPath = path }
            if let value = arguments["population"] { population = try integer(value, name: "population") }
            if let value = arguments["generations"] { generations = try integer(value, name: "generations") }
            if let task = arguments["task"] as? String { isRegression = task.lowercased() == "regression" }
            inspectDataset()
            guard dataset != nil else { throw ControlFailure.message(status) }
            selectedPage = .train
            startTraining()
            guard isTraining else { throw ControlFailure.message(status) }
        case "stop": stopTraining()
        case "predict":
            predictionInput = try requiredString(arguments, "input")
            if let path = arguments["model"] as? String { modelPath = path }
            selectedPage = .useModel
            predict()
        default:
            throw ControlFailure.message("unknown command: \(command)")
        }
    }

    private func setControlValue(name: String, value: Any) throws {
        switch name {
        case "isRegression": isRegression = try boolean(value, name: name)
        case "hasHeader": hasHeader = try boolean(value, name: name)
        case "targetColumns": targetColumns = try integer(value, name: name)
        case "population": population = try integer(value, name: name)
        case "generations": generations = try integer(value, name: name)
        case "elite": elite = try integer(value, name: name)
        case "threads": threads = try integer(value, name: name)
        case "seed": seed = try unsigned(value, name: name)
        case "initialHidden": initialHidden = try integer(value, name: name)
        case "patience": patience = try integer(value, name: name)
        case "targetScore": targetScore = try number(value, name: name)
        case "weightRate": weightRate = try number(value, name: name)
        case "weightSigma": weightSigma = try number(value, name: name)
        case "topologyRate": topologyRate = try number(value, name: name)
        case "complexityPenalty": complexityPenalty = try number(value, name: name)
        case "generatedRows": generatedRows = try integer(value, name: name)
        case "generatedInputs": generatedInputs = try integer(value, name: name)
        case "generatedMinimum": generatedMinimum = try number(value, name: name)
        case "generatedMaximum": generatedMaximum = try number(value, name: name)
        case "generatedNoise": generatedNoise = try number(value, name: name)
        case "generatedSeed": generatedSeed = try unsigned(value, name: name)
        case "imputeMissing": imputeMissing = try boolean(value, name: name)
        case "removeDuplicates": removeDuplicates = try boolean(value, name: name)
        case "dropMalformed": dropMalformed = try boolean(value, name: name)
        case "clipOutliers": clipOutliers = try boolean(value, name: name)
        case "clipZScore": clipZScore = try number(value, name: name)
        case "predictionInput": predictionInput = try string(value, name: name)
        case "dataPath": dataPath = try string(value, name: name)
        case "outputPath": outputPath = try string(value, name: name)
        case "modelPath": modelPath = try string(value, name: name)
        case "syntheticPattern":
            let requested = try string(value, name: name)
            guard let result = pattern(named: requested) else {
                throw ControlFailure.message("unknown pattern: \(requested)")
            }
            syntheticPattern = result
        default:
            throw ControlFailure.message("setting is not controllable: \(name)")
        }
    }

    private func controlState() -> [String: Any] {
        var state: [String: Any] = [
            "page": selectedPage?.rawValue ?? "Welcome",
            "status": status,
            "backend": backend,
            "dataPath": dataPath,
            "outputPath": outputPath,
            "modelPath": modelPath,
            "isRegression": isRegression,
            "hasHeader": hasHeader,
            "targetColumns": targetColumns,
            "population": population,
            "generations": generations,
            "elite": elite,
            "threads": threads,
            "seed": seed,
            "initialHidden": initialHidden,
            "patience": patience,
            "targetScore": targetScore,
            "weightRate": weightRate,
            "weightSigma": weightSigma,
            "topologyRate": topologyRate,
            "complexityPenalty": complexityPenalty,
            "syntheticPattern": syntheticPattern.rawValue,
            "generatedRows": generatedRows,
            "generatedNoise": generatedNoise,
            "isTraining": isTraining,
            "generation": generation,
            "progress": progress,
            "elapsed": elapsed,
            "trainScore": trainScore,
            "validationScore": validationScore,
            "testScore": jsonMetric(testScore),
            "testLoss": jsonMetric(testLoss),
            "baselineScore": jsonMetric(baselineScore),
            "baselineLoss": jsonMetric(baselineLoss),
            "improvementOverBaseline": jsonMetric(improvementOverBaseline),
            "balancedAccuracy": jsonMetric(balancedAccuracy),
            "meanAbsoluteError": jsonMetric(meanAbsoluteError),
            "rSquared": jsonMetric(rSquared),
            "parameters": parameterCount,
            "evaluations": evaluations,
            "topology": topology,
            "predictionInput": predictionInput,
            "predictionOutput": predictionOutput
        ]
        if let dataset {
            state["dataset"] = ["inputs": dataset.inputs, "outputs": dataset.outputs,
                                "classes": dataset.classes, "train": dataset.train,
                                "validation": dataset.validation, "test": dataset.test]
        }
        if let profile = qualityProfile {
            state["quality"] = ["rows": profile.rows, "columns": profile.columns,
                                "completeRows": profile.completeRows,
                                "missingCells": profile.missingCells,
                                "malformedRows": profile.malformedRows,
                                "duplicateRows": profile.duplicateRows,
                                "score": profile.qualityScore]
        }
        return state
    }

    private func jsonMetric(_ value: Double?) -> Any {
        guard let value, value.isFinite else { return NSNull() }
        return value
    }

    private func pattern(named name: String) -> SyntheticPattern? {
        let normalized = name.lowercased().replacingOccurrences(of: "-", with: " ")
        return SyntheticPattern.allCases.first {
            $0.rawValue.lowercased() == normalized ||
            ($0 == .sine && normalized == "sine")
        }
    }

    private func requiredString(_ values: [String: Any], _ name: String) throws -> String {
        guard let value = values[name] else { throw ControlFailure.message("missing argument: \(name)") }
        return try string(value, name: name)
    }
    private func string(_ value: Any, name: String) throws -> String {
        guard let result = value as? String, !result.isEmpty else {
            throw ControlFailure.message("\(name) must be a non-empty string")
        }
        return result
    }
    private func integer(_ value: Any, name: String) throws -> Int {
        guard let number = value as? NSNumber, !isJSONBoolean(number),
              number.doubleValue.isFinite,
              number.doubleValue.rounded(.towardZero) == number.doubleValue,
              number.doubleValue >= Double(Int.min), number.doubleValue <= Double(Int.max) else {
            throw ControlFailure.message("\(name) must be an integer")
        }
        return number.intValue
    }
    private func unsigned(_ value: Any, name: String) throws -> UInt64 {
        guard let number = value as? NSNumber, !isJSONBoolean(number),
              number.doubleValue.isFinite, number.doubleValue >= 0,
              number.doubleValue.rounded(.towardZero) == number.doubleValue else {
            throw ControlFailure.message("\(name) must be a nonnegative integer")
        }
        return number.uint64Value
    }
    private func number(_ value: Any, name: String) throws -> Double {
        guard let number = value as? NSNumber, !isJSONBoolean(number),
              number.doubleValue.isFinite else {
            throw ControlFailure.message("\(name) must be a finite number")
        }
        return number.doubleValue
    }
    private func boolean(_ value: Any, name: String) throws -> Bool {
        guard let number = value as? NSNumber, isJSONBoolean(number) else {
            throw ControlFailure.message("\(name) must be true or false")
        }
        return number.boolValue
    }
    private func isJSONBoolean(_ number: NSNumber) -> Bool {
        CFGetTypeID(number) == CFBooleanGetTypeID()
    }
    private func writeControlJSON(_ object: [String: Any], to url: URL) throws {
        let data = try JSONSerialization.data(withJSONObject: object, options: [.prettyPrinted, .sortedKeys])
        try data.write(to: url, options: .atomic)
    }
}
