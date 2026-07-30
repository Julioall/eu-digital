# Episodic memory promotion contract

The product contract for stored records remains
`contracts/schemas/episode.schema.json`. This envelope defines the
cross-language evaluation of episodic memory.

Each fixture line contains `case_id`, `records`, `query`, and optionally
`max_episodes`, `minimum_relation_score`, `consolidate`, `relevance` and
`invariants`. Each record contains a complete Episode and may contain an
optional local embedding. An embedding is an evaluation signal, not a model or
a runtime dependency.

The result contains `store_results`, `size`, `retrieval`, `relations` and
`consolidated`. Each retrieval result preserves the original episode,
`reason_codes`, a deterministic explanation and provenance with `episode_id`,
`event_ids`, `created_by` and `schema_version`. Each relation contains both
episode IDs, a score, explicit reasons and source event IDs.

`relevance` and `invariants` are evaluation metadata and are not read by the
candidate. The holdout is kept in `validation/holdout/` and its hash is frozen
in the promotion manifest before execution.

The promotion manifest also records `reference.source_path` and
`reference.source_sha256` for the frozen Python entrypoint. Both input episodes
and every retrieved native episode are validated against
`contracts/schemas/episode.schema.json` before evidence is accepted.

The chronological baseline and the context/embedding-disabled ablation are
reported separately from treatment metrics. Agreement between implementations
is computational verification only; it does not establish cognitive validity.
