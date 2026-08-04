# Contratos de saída cognitiva estruturada

Os schemas executáveis da SPEC-048 são normativos em `contracts/schemas/`:

- `cognitive_output_request.schema.json`: intenção semântica derivada de uma
  decisão já concluída, conteúdo explícito do input, restrições funcionais e
  referências permitidas;
- `language_rendering_candidate.schema.json`: único formato aceito de um
  renderer local;
- `cognitive_output.schema.json`: envelope validado entregue à porta de
  apresentação.

O request não é parte de `CognitiveCycleResult` 1.0 e não existe em replay. O
renderer não decide se deve falar, não consulta memória por conta própria e não
executa ações. `evidence_refs` do candidato deve ser subconjunto das referências
do request.

Estados com texto são somente `rendered` e `fallback_used`. `silence`,
`malformed`, `timeout` e `unavailable` têm texto vazio e não são apresentados.
O fallback é uma mensagem fixa de indisponibilidade para requests críticos; ele
não é resposta factual ao conteúdo solicitado.

As portas C++ são capacidades opcionais `language.render` e
`presentation.present`. Sua ausência, falha ou substituição não altera memória,
decisão ou self-model, exceto pelo mecanismo normal de estado de capacidades.
