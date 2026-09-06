# Neuroevo Studio Quality Matrix

This matrix is the release contract for the educational lab and the real-world workbench. “Automated” means a repeatable test must run in CI; “manual” means the release runbook must record the result. Unchecked rows are planned work, not claimed capability.

## Data ingestion

| Case | Expected behavior | Owner |
|---|---|---|
| Empty, header-only, or fewer than 12 rows | Actionable rejection without a partial model | Automated |
| CRLF, UTF-8 BOM, blank lines, surrounding whitespace | Parse consistently or explain the unsupported form | Automated |
| Quoted CSV fields, embedded commas, and escaped quotes | Parse correctly after the RFC-compatible reader lands | Planned |
| Missing features, missing targets, malformed numbers | Profile exact counts; never invent a target | Automated |
| `NaN`, infinities, overflow, and underflow | Reject non-finite training values | Automated |
| Constant features or targets | Remain numerically stable and explain metric limits | Automated |
| Duplicate rows | Report and optionally remove without changing the source | Automated |
| Highly imbalanced or rare classes | Proportional deterministic split preserves sufficiently represented classes; warn when evaluation is invalid | Automated + warning planned |
| Very wide or very large files | Enforce documented resource limits; benchmark representative sizes | Planned |
| Unreadable input or unwritable output | Fail with the affected path and preserve prior artifacts | Automated |

## Evaluation and trust

| Case | Expected behavior | Owner |
|---|---|---|
| Classification | Accuracy, balanced accuracy, and majority baseline | Automated |
| Regression | MSE, MAE, R-squared, and train-mean baseline | Automated |
| Model no better than baseline | Visible warning; no “successful model” language | Automated + UI |
| Train/validation divergence | Generalization warning and educational explanation | Planned |
| Missing class in an evaluation split | Metric limitation is explicit; split can be corrected | Planned |
| Constant regression target | Finite documented R-squared behavior | Automated |
| Repeated seed/config/data | Same split and selected result across thread counts | Automated |
| Test set | Evaluated once after selection and clearly distinguished from validation | Automated |

## Engine, model, and configuration

| Case | Expected behavior | Owner |
|---|---|---|
| Zero/invalid population, elite, tournament, or dimensions | Reject before allocation or worker launch | Automated |
| Non-finite or out-of-range mutation settings | Reject with the setting name | Automated |
| Excessive dimensions or counts | Bounded by documented limits before allocation | Planned |
| Truncated, corrupt, unsupported, or non-finite model | Reject before prediction | Planned |
| Feature-count mismatch | Actionable prediction error | Automated |
| Cancellation | Finish the active safe boundary and preserve a valid winner | Automated |
| Worker exception | Propagate to the caller without deadlock | Automated |
| Scalar vs Accelerate | Numerically equivalent within tolerance | Automated on macOS |

## Native app and accessibility

| Case | Expected behavior | Owner |
|---|---|---|
| Invalid fields and impossible combinations | Inline validation; training remains disabled | Planned Swift tests |
| Repeated Train/Stop, quit during training | No duplicate worker, crash, or corrupt model | Planned UI tests |
| CLI changes during GUI interaction | Main-actor-safe update with structured reply | Existing integration test |
| Keyboard-only workflow | Every primary action reachable with visible focus | Manual + planned UI tests |
| VoiceOver, larger text, contrast, reduce motion | Main tasks remain understandable and operable | Manual audit |
| Dark/light and active/inactive windows | Legible state and hierarchy | Manual visual QA |
| Error recovery | Error identifies the fix and preserves user-entered settings | Planned Swift tests |

## Privacy, security, and release

| Case | Expected behavior | Owner |
|---|---|---|
| Offline use | Core workflow performs no network requests | Code review + manual |
| Local CLI control | Per-user permissions, bounded commands, no network listener | Existing integration test |
| Source data | Cleaning never overwrites source; no secret collection | Automated + review |
| Dependency/build integrity | Minimal workflow permissions and pinned major actions | CI review |
| Clean supported Mac | App launches, trains sample, predicts, exports, and uninstalls cleanly | Release manual |
| DMG architecture/signature/license | Verification script passes | Existing automated script |
| Notarization | Required before public binary distribution | Blocked on Developer ID credentials |

## Real-workload acceptance set

- Balanced and 99:1 imbalanced binary classification.
- Three-class classification with overlapping classes and label noise.
- Linear, nonlinear, multi-output, constant-target, and noise-only regression.
- Small data near the minimum; 100,000-row standard workload; bounded stress workload.
- Distribution shift: test inputs outside the training range.
- A deliberately unlearnable dataset where the correct outcome is “not better than baseline.”

## Release gate

A release is “educational preview” until every trust-foundation row is automated, preprocessing is leakage-resistant, and the main accessibility path is audited. A release is “production candidate” only after experiment reports, schema validation, recovery testing, clean-machine testing, signing, and notarization pass.
