# Decision: Trust Evidence Before AI Assistance

## Date
2026-09-06

## Context
Code generation is widely available, so Neuroevo Studio cannot differentiate through generated model code alone. Its durable value must be a workflow that helps people form a valid problem, understand data, run a reproducible experiment, recognize failure, and decide whether a model is useful.

Apple’s [machine-learning design guidance](https://developer.apple.com/design/human-interface-guidelines/machine-learning) emphasizes realistic expectations, limitations, correction, confidence, and designing for mistakes. The [NIST AI Risk Management Framework](https://www.nist.gov/publications/artificial-intelligence-risk-management-framework-ai-rmf-10) and [Measure guidance](https://airc.nist.gov/airmf-resources/playbook/measure/) emphasize documented test/evaluation evidence and examination of failure pockets rather than relying only on aggregate performance. Apple’s [accessibility testing guidance](https://developer.apple.com/documentation/accessibility/performing-accessibility-testing-for-your-app) makes the primary workflow—not a decorative accessibility checklist—the unit that must be tested.

## Options Considered
1. Add a general chat assistant first.
2. Add more network architectures and fashionable model types first.
3. Establish deterministic trust evidence and teach directly from it before adding optional assistance.

## Decision
Choose option 3. Every trained model must first be compared with a credible naive baseline, display task-appropriate metrics, distinguish validation from held-out testing, and describe limitations honestly. Guided Lab and Workbench will consume the same measured experiment evidence. Any future AI coach must be optional, local-first where practical, grounded in that evidence and the versioned manual, and clearly distinguish measured facts from generated suggestions.

## Reason
- A model that does not beat a trivial baseline is not useful merely because training completed.
- The same baseline comparison is both professional evaluation practice and a powerful educational lesson.
- Deterministic evidence is testable, exportable, and reusable by humans or future assistants.
- Optional assistance can improve explanation later without becoming a hidden dependency or source of invented results.

## Risk
- Baselines can be too weak for a domain and must become configurable.
- Aggregate balanced accuracy and R-squared still do not expose calibration, subgroup harm, or asymmetric costs.
- Added rigor increases the apparent complexity of the beginner workflow unless progressive disclosure is maintained.

## Rollback
The Trust Report UI can be removed without changing saved model format. Do not remove the evaluator tests or revert to describing every completed run as successful; replace metrics only with stronger documented evidence.
