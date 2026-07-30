# Local Model Gateway Contract

The executable schemas of SPEC-013 are local and do not identify a concrete
runtime or model:

- `model_prompt_template.schema.json`: immutable template by ID and version;
- `local_model_request.schema.json`: queued request with local backend/model,
  priority, timeout and rendered prompt;
- `local_model_response.schema.json`: completed response with `output.kind`
  and `output.fields` validated before return.

Prompt content remains in the local process. This contract does not authorize
network delivery, actions, autonomous dialogue or an API. Output that does not
respect the structured format is rejected and is not converted to free text.

## SPEC-040 native boundary

The C++ implementation is an optional, backend-agnostic port. It owns one
heavy worker, stable priority/FIFO selection, timeout and cancellation
forwarding, unload-after-request, structured response validation and an
availability record that keeps timeline, privacy and diagnostics independent
from model availability.

`LocalModelBackend` is intentionally injectable. The repository does not
download, bundle or select a concrete model runtime. A fixture artifact is
validated as GGUF, Portuguese-compatible, license-compatible, at most 4 GiB,
hash-matching and compatible with a declared backend. Runtime and payload IDs
must differ and are bound by the local detached-manifest-digest envelope.
This envelope is an integrity/test contract, not a claim of asymmetric release
authentication; release signing requires a future decision recorded by ADR.

The native promotion command is:

```text
promotion_fixture_runner --local-model-dialogue
```

It accepts JSON Lines fixtures only and emits no prompt contents in gateway
audit or metrics state. A missing or invalid artifact disables dialogue while
leaving local operational capabilities available.
