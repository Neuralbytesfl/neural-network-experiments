import SwiftUI

private struct ManualTopic: Identifiable, Hashable {
    let id: String
    let title: String
    let summary: String
    let symbol: String
    let sections: [ManualSection]
}

private struct ManualSection: Hashable {
    let title: String
    let body: String
}

struct LearnView: View {
    @State private var query = ""
    @State private var selectedID = "quick-start"

    private var filtered: [ManualTopic] {
        guard !query.isEmpty else { return topics }
        return topics.filter { topic in
            ([topic.title, topic.summary] + topic.sections.flatMap { [$0.title, $0.body] })
                .joined(separator: " ").localizedCaseInsensitiveContains(query)
        }
    }

    var body: some View {
        HStack(spacing: 0) {
            VStack(alignment: .leading, spacing: 12) {
                Text("LEARNING MANUAL").font(.caption.bold()).foregroundStyle(.mint)
                TextField("Search concepts", text: $query).textFieldStyle(.roundedBorder)
                List(filtered, selection: $selectedID) { topic in
                    Label {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(topic.title)
                            Text(topic.summary).font(.caption2).foregroundStyle(.secondary).lineLimit(2)
                        }
                    } icon: { Image(systemName: topic.symbol) }
                    .tag(topic.id)
                }.listStyle(.sidebar)
            }.padding(18).frame(width: 310)
            Divider()
            if let topic = topics.first(where: { $0.id == selectedID }) ?? filtered.first {
                ScrollView {
                    VStack(alignment: .leading, spacing: 22) {
                        Image(systemName: topic.symbol).font(.system(size: 34)).foregroundStyle(.mint)
                        Text(topic.title).font(.system(size: 34, weight: .bold, design: .rounded))
                        Text(topic.summary).font(.title3).foregroundStyle(.secondary)
                        ForEach(topic.sections, id: \.self) { section in
                            StudioPanel(section.title, subtitle: "") {
                                Text(section.body).font(.body).lineSpacing(5).textSelection(.enabled)
                            }
                        }
                    }.padding(32).frame(maxWidth: 900, alignment: .leading)
                }.frame(maxWidth: .infinity)
            } else {
                ContentUnavailableView.search(text: query).frame(maxWidth: .infinity)
            }
        }
    }

    private let topics: [ManualTopic] = [
        ManualTopic(id: "quick-start", title: "Quick Start", summary: "Create, prepare, train, and use your first model.", symbol: "figure.walk",
                    sections: [
                        ManualSection(title: "The four-step workflow", body: "1. Create Data: generate a controlled pattern or import numeric CSV.\n\n2. Prepare Data: inspect missing cells, malformed rows, and duplicates; create a cleaned copy.\n\n3. Train Model: set a population and generation budget, then watch validation performance.\n\n4. Use Model: enter features in the same order as training or export the winner to Core ML."),
                        ManualSection(title: "A reliable first experiment", body: "Choose Circles, generate 1,000 rows with seed 42 and modest noise, keep the default training controls, and start evolution. The problem is nonlinear, so it demonstrates why hidden layers matter. Repeat with the same seed to reproduce the dataset and model search."),
                        ManualSection(title: "What success means", body: "Success is not a perfect training score. Look for strong validation performance, a similar held-out test score, and behavior that remains acceptable on realistic inputs. Record the seed, settings, data version, and resulting model together.")
                    ]),
        ManualTopic(id: "data-labels", title: "Data & Labels", summary: "Turn a question into numeric examples.", symbol: "tablecells",
                    sections: [
                        ManualSection(title: "Rows, features, and targets", body: "Each CSV row is one example. Input columns are features the model receives. The final target column—or columns—is the answer it must learn. Keep units and feature order consistent at training and prediction time."),
                        ManualSection(title: "Classification versus regression", body: "Classification chooses among categories such as 0 and 1. Regression estimates continuous values such as temperature or demand. Use integer class labels for classification. Use numeric target values for regression."),
                        ManualSection(title: "Representative examples", body: "A model can only learn the relationships represented in its data. Include ordinary cases, rare but important cases, realistic noise, and the environments where the model will operate. More rows do not repair systematically biased collection.")
                    ]),
        ManualTopic(id: "synthetic", title: "Synthetic Data", summary: "Use controlled patterns without confusing them for reality.", symbol: "sparkles.rectangle.stack",
                    sections: [
                        ManualSection(title: "Why generate data", body: "Synthetic patterns are useful for learning, debugging, benchmarking, and testing whether a model family can express a known relationship. A seed makes a random-looking dataset repeatable."),
                        ManualSection(title: "Patterns in this app", body: "Linear, polynomial, and sine patterns are regression tasks. XOR, circles, clusters, and spiral patterns are classification tasks. Noise makes the boundary less exact and helps reveal whether evolution generalizes rather than memorizes."),
                        ManualSection(title: "The limitation", body: "Generated data reflects assumptions written into the generator. It cannot prove performance on people, markets, machines, nature, or any other real system unless the generator itself has been validated against that system.")
                    ]),
        ManualTopic(id: "cleaning", title: "Cleaning & Leakage", summary: "Repair structural problems while preserving evidence.", symbol: "wand.and.stars",
                    sections: [
                        ManualSection(title: "Conservative cleaning", body: "This app preserves the source and writes a new CSV. It can fill missing feature values with column means, drop missing targets, remove exact duplicates, drop rows with the wrong shape, and optionally clip extreme feature values."),
                        ManualSection(title: "Why targets are never invented", body: "A missing target means the correct answer is unknown. Filling it from the other rows would teach the model a guessed label as if it were fact, so the cleaner drops that row."),
                        ManualSection(title: "Avoid leakage", body: "For production, split first. Fit means, clipping bounds, encoders, and every learned preprocessing value using only the training split. Apply those frozen values to validation, test, and live inputs. Cleaning the complete dataset can leak information across splits.")
                    ]),
        ManualTopic(id: "splits", title: "Train, Validate, Test", summary: "Give every dataset split exactly one job.", symbol: "square.split.2x1",
                    sections: [
                        ManualSection(title: "Training split", body: "Evolution uses training rows to adjust weights and structure. Repeated exposure means training performance is optimistic."),
                        ManualSection(title: "Validation split", body: "Validation rows compare candidate networks and guide early stopping. Because you use this score to make choices, it is part of model development."),
                        ManualSection(title: "Test split", body: "The test split estimates final generalization after choices are complete. Do not repeatedly tune against it. Time-dependent data often requires chronological splitting rather than random splitting.")
                    ]),
        ManualTopic(id: "evolution", title: "How Evolution Works", summary: "Networks compete, mutate, and pass useful traits forward.", symbol: "point.3.connected.trianglepath.dotted",
                    sections: [
                        ManualSection(title: "Population", body: "A generation contains many candidate neural networks. Each candidate has a topology and numeric weights. A larger population explores more possibilities but costs more evaluations."),
                        ManualSection(title: "Selection and mutation", body: "High-performing candidates survive as elites or become parents. Offspring receive small weight changes and occasional topology changes. This produces a search over both parameters and structure without gradient descent."),
                        ManualSection(title: "Fitness", body: "Fitness rewards predictive score and applies a small complexity penalty. This favors useful accuracy while discouraging unnecessarily large networks. Validation—not training alone—selects the saved winner.")
                    ]),
        ManualTopic(id: "metrics", title: "Metrics & Overfitting", summary: "Read the chart without fooling yourself.", symbol: "chart.xyaxis.line",
                    sections: [
                        ManualSection(title: "Score direction", body: "Classification score is accuracy: higher is better. Regression score is derived from error so higher is also better in the dashboard. Always inspect domain-relevant error measures before deployment."),
                        ManualSection(title: "Overfitting signal", body: "If training improves while validation stalls or declines, the search is fitting details that do not generalize. Consider stopping earlier, reducing topology mutation, increasing representative data, or strengthening the complexity penalty."),
                        ManualSection(title: "Beyond one number", body: "Accuracy can hide expensive mistakes or class imbalance. Real evaluations may need precision, recall, confusion matrices, calibration, subgroup analysis, latency, memory, and cost-weighted errors.")
                    ]),
        ManualTopic(id: "production", title: "Using Models Safely", summary: "Preserve the data contract and plan for failure.", symbol: "shippingbox",
                    sections: [
                        ManualSection(title: "Input contract", body: "Predictions require the same number, order, units, and preprocessing of features used during training. Validate every live input and define behavior for missing, extreme, and out-of-distribution values."),
                        ManualSection(title: "Version and monitor", body: "Version the model with its data schema, preprocessing, evaluation report, and seed. Monitor performance and data drift after release. Keep the previous known-good model available for rollback."),
                        ManualSection(title: "Human responsibility", body: "Models are fallible estimates. High-impact uses need expert review, privacy protection, bias evaluation, explainable escalation paths, and safeguards appropriate to the consequences of an error.")
                    ]),
        ManualTopic(id: "coreml", title: "Core ML & Apple Silicon", summary: "Understand export and Neural Engine eligibility.", symbol: "apple.logo",
                    sections: [
                        ManualSection(title: "Export", body: "The exporter translates the evolved feed-forward network into a Core ML model. Your application supplies a float feature array and reads the prediction array."),
                        ManualSection(title: "CPU and Neural Engine", body: "Neuroevolution training runs in optimized native C++ and uses Apple Accelerate where appropriate. The exported Core ML model can request CPU and Neural Engine compute. Core ML ultimately schedules operations based on support and device availability."),
                        ManualSection(title: "Performance practice", body: "Measure on the target Mac or iPhone with realistic batch sizes. A tiny network may run faster on CPU because dispatch overhead dominates. Hardware eligibility alone is not a benchmark.")
                    ]),
        ManualTopic(id: "automation", title: "CLI Automation", summary: "Control the open GUI from scripts in real time.", symbol: "terminal",
                    sections: [
                        ManualSection(title: "Why it exists", body: "The studioctl command changes the same observable state used by SwiftUI. A script can navigate, update settings, generate or clean data, train, stop, and predict while the open window reflects each command within roughly a tenth of a second."),
                        ManualSection(title: "Examples", body: "Run scripts/studioctl status to read structured JSON. Use page create to navigate, set generatedRows 600 to change a control, generate --pattern circles --rows 600 --output /tmp/circles.csv to create data, and watch to stream training metrics."),
                        ManualSection(title: "Local security", body: "Commands use a private per-user directory under /tmp. There is no network listener. Only explicitly supported commands and setting names are accepted, and file-producing actions require an explicit destination path.")
                    ]),
        ManualTopic(id: "troubleshooting", title: "Troubleshooting", summary: "Resolve common data, training, and export problems.", symbol: "wrench.and.screwdriver",
                    sections: [
                        ManualSection(title: "Dataset rejected", body: "Check that every data cell is numeric, the header setting matches the file, each row has the same column count, there are enough rows for all splits, and target columns are at the end."),
                        ManualSection(title: "Training does not improve", body: "Confirm the task type and labels, inspect the preview, lower noise, increase population or generations gradually, and compare multiple fixed seeds. A relationship may be absent from the selected features."),
                        ManualSection(title: "Core ML export fails", body: "Install the documented Python Core ML dependency environment, ensure the saved model is readable, and use the release verification script. Export availability does not affect native C++ prediction.")
                    ])
    ]
}
