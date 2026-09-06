# Runbook: Build and Distribute Neuroevo Studio

## Purpose
Produce a reproducible ARM64 `.app` and compressed DMG, verify their structure and signature, and optionally sign/notarize them for public distribution.

## Prerequisites
- macOS 14 or newer
- Swift 6 / Apple Command Line Tools
- Built-in `sips`, `iconutil`, `codesign`, and `hdiutil`
- For public distribution only: an Apple Developer Program Developer ID Application certificate and a configured `notarytool` keychain profile

## Local Test Package
```bash
./scripts/build-macos-app --dry-run
./scripts/build-dmg --dry-run
./scripts/build-dmg
./scripts/verify-release dist/NeuroevoStudio.dmg
open dist/NeuroevoStudio.dmg
```

The default signature is ad-hoc. This validates bundle integrity locally but does not satisfy Gatekeeper distribution requirements on another Mac.

## Developer ID and Notarization
Store notarization credentials in the login keychain with `xcrun notarytool store-credentials`; never place credentials in this repository. Then run:

```bash
./scripts/build-dmg \
  --identity "Developer ID Application: YOUR ORGANIZATION (TEAMID)" \
  --bundle-id "YOUR.REVERSE.DNS.ID" \
  --version "1.0.0" \
  --notary-profile "YOUR_EXISTING_KEYCHAIN_PROFILE"
./scripts/verify-release dist/NeuroevoStudio.dmg
```

Replace the placeholders with existing local values. Use a bundle identifier registered to your developer account. The script enables hardened runtime for the app, timestamps signatures, submits the DMG, waits for Apple notarization, and staples the ticket.

## Verification
- `plutil` validates the bundled property list.
- `codesign --verify --deep --strict` validates the app signature.
- `file` confirms an ARM64 executable.
- `hdiutil verify` validates the disk-image checksum.
- The mounted image must include `NeuroevoStudio.app` and an Applications symlink.

## Rollback
Release artifacts are generated under `dist/` and ignored by Git. Delete only the specific generated `.app` and `.dmg`, then rebuild the previous Git revision. Source rollback is `git revert <productization-commit>`.

## Troubleshooting
- **Signing identity not found:** run `security find-identity -v -p codesigning` and use an exact Developer ID Application identity.
- **Notarization rejected:** inspect the submission log with `xcrun notarytool log` and fix the reported signature or bundle issue.
- **Icon missing:** run `./scripts/build-iconset` and verify `build/AppIcon.icns` exists.
