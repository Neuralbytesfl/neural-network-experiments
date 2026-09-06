import CoreML
import Foundation

@main
enum VerifyCoreML {
    static func main() throws {
        guard CommandLine.arguments.count >= 3 else {
            FileHandle.standardError.write(
                Data("usage: verify_coreml MODEL.mlmodel x1,x2,...\n".utf8)
            )
            Foundation.exit(2)
        }
        let values = CommandLine.arguments[2].split(separator: ",").compactMap {
            Float($0.trimmingCharacters(in: .whitespaces))
        }
        guard !values.isEmpty else { throw VerificationError.invalidFeatures }

        let source = URL(fileURLWithPath: CommandLine.arguments[1])
        let compiled = try MLModel.compileModel(at: source)
        let configuration = MLModelConfiguration()
        configuration.computeUnits = .cpuAndNeuralEngine
        let model = try MLModel(contentsOf: compiled, configuration: configuration)
        let input = try MLMultiArray(shape: [NSNumber(value: values.count)], dataType: .float32)
        for (index, value) in values.enumerated() { input[index] = NSNumber(value: value) }
        let provider = try MLDictionaryFeatureProvider(dictionary: ["features": input])
        let result = try model.prediction(from: provider)
        guard let output = result.featureValue(for: "prediction")?.multiArrayValue else {
            throw VerificationError.missingOutput
        }
        let rendered = (0..<output.count).map { String(format: "%.8g", output[$0].doubleValue) }
        print("compute_units=cpuAndNeuralEngine output=[\(rendered.joined(separator: ","))]")
    }

    enum VerificationError: Error {
        case invalidFeatures
        case missingOutput
    }
}
