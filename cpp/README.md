# Cérebro Implantado C++

Este diretório contém o runtime final instalável. Ele não pode depender de Python em execução.

Consulte:

- `docs/02-architecture/LAB_AND_DEPLOYED_BRAIN_ARCHITECTURE.md`
- `docs/04-adrs/ADR-0010-python-laboratory-cpp-deployed-brain.md`
- `specs/SPEC-025-hybrid-monorepo-foundation.md`

O executável mínimo lê `contracts/fixtures/canonical_event.json`. O build não
carrega interpretador Python nem módulos do laboratório.

O registry e o lifecycle nativos da SPEC-023 estão em
`cpp/core/capability_runtime.hpp`, com teste em
`cpp/tests/capability_runtime_test.cpp`. A descoberta de manifestos e entry
points é exercitada na referência Python; o runtime C++ consome descritores e
plugins nativos por interfaces abstratas.

The SPEC-026 native promotion boundary is represented by
`cpp/app/promotion_fixture_runner.cpp`, which preserves fixture bytes, and
`cpp/core/promotion_registry.hpp`, which exposes the approved promotion for a
component. The fixture runner is transport infrastructure; it does not claim
that a cognitive mechanism has been promoted.
The initial SPEC-003 system activity sensor is in
`cpp/core/system_activity_sensor.hpp`. Its normalization and lifecycle logic
is platform-independent, while `WindowsSystemActivityAdapter` encapsulates
Win32 process enumeration and foreground-window polling behind the adapter
port. The Linux test uses a fake adapter and never treats unavailable
observation as a negative observation.
The initial SPEC-004 interaction sensor is in
`cpp/core/input_interaction_sensor.hpp`. It accepts raw keyboard/pointer and
clipboard callbacks, emits versioned payloads, aggregates counters without
discarding metrics, associates events with the supplied active-window context,
and does not interpret intent or execute input actions.
The initial SPEC-005 screen/OCR sensor is in
`cpp/core/screen_ocr_sensor.hpp`. It receives frames from a local capture
adapter, stores image bytes behind `ImageStore`, applies an average-hash
deduplication policy with an interval fallback, and invokes an injected local
`OcrEngine`. Visual and OCR events reference the stored image and persist word
coordinates; event payloads never duplicate screen pixels. OCR failures keep
the visual event and expose structured health state.
The SPEC-006 timeline store is in `cpp/core/timeline_store.hpp`. It uses a
local SQLite file with versioned migrations, append-only event IDs, temporal
and context indexes, deterministic pagination, JSON export, and ordered replay.
Session, application, and correlation metadata are stored alongside the
canonical event without changing the event contract.
