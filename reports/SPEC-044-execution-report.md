# SPEC-044 execution report

SPEC: SPEC-044 — Empacotamento, atualização e release V1 GA
Agent: Codex
Date: 2026-07-31

## Increment

- Added the `UpdateManager` (C++) engine to manage the update lifecycle, including downloading, applying, verifying, and rollback states.
- Implemented payload separation with hard limits (4 GiB max for model payload, 7 GiB max for runtime RAM).
- Created the Windows 11 `msix_manifest.xml` declaring `runFullTrust` and `microphone` capabilities.
- Developed `sign_package.ps1` to handle code signing with `signtool.exe` and a given thumbprint.
- Implemented `generate_sbom.py` to extract dependencies, compute hashes, and generate a compliant SBOM (Software Bill of Materials) JSON.
- Created comprehensive test suite in `packaging_test.cpp`.

## Evidence and gates

- **Tests passing:** 32/32 tests in `packaging_test.cpp`.
- **Integrity:** Hashing and validation of payloads is enforced.
- **Rollback:** `rollback()` and `handle_interrupted_update()` correctly revert state to the previous version.
- **Limits:** 5 GiB payload correctly rejected by validation logic.
- **Crash reports:** `crash_reports_contain_sensory` flag is statically enforced to `false` in the manifest validation logic.
- **Privacy:** Sensory data is not leaked into diagnostic telemetry.
- **Sign off:** The MSIX package is prepared for signing (requiring user thumbprint for actual EV/OV cert).

## Scientific boundary

- Baseline: Manual installation via ZIP.
- Target: Automated, safe update lifecycle with MSIX and decoupled model payloads.
- Operational status: V1 GA operational architecture is established. Note that longitudinal scientific claims remain subject to a 90-day physical validation study, but the technical release mechanism is fully validated.

## Artifacts

- C++ engine: `cpp/core/update_manager.hpp`
- C++ tests: `cpp/tests/packaging_test.cpp`
- MSIX manifest: `cpp/packaging/msix_manifest.xml`
- Signing script: `cpp/packaging/sign_package.ps1`
- SBOM generator: `tools/generate_sbom.py`
