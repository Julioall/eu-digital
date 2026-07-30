# Contrato de promoção: memória episódica

O contrato de produto dos registros armazenados continua sendo
`contracts/schemas/episode.schema.json`. Este envelope define a avaliação
cross-language da memória.

Cada linha de fixture contém `case_id`, `records`, `query`, opcionalmente
`max_episodes`, `minimum_relation_score`, `consolidate`, `relevance` e
`invariants`. Cada record tem um `episode` completo e pode ter `embedding`; o
embedding é um vetor local de avaliação, não um modelo nem uma dependência do
runtime.

Fixture SHA-256 values are computed after normalizing `CRLF` to `LF`, so the
frozen hashes remain independent of Git newline conversion on Windows.

O resultado contém `store_results`, `size`, `retrieval`, `relations` e
`consolidated`. Cada resultado de recuperação preserva o episódio original,
`reason_codes`, uma explicação determinística e `provenance` com
`episode_id`, `event_ids`, `created_by` e `schema_version`. Cada relação contém
os dois IDs, score, razões e os eventos de origem.

`relevance` e `invariants` são metadados de avaliação e não são lidos pelo
candidato. O holdout é mantido em `validation/holdout/` e sua hash é congelada
no manifesto da promoção antes da execução.
