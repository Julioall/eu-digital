# Contratos: Workspace Global

Os schemas executáveis e versionados da SPEC-010 ficam em
`contracts/schemas/`:

- `workspace_candidate.schema.json`: entrada genérica e local de informação
  observada; requer referências de origem e pelo menos um sinal fornecido;
- `workspace_item.schema.json`: item selecionado, com score, fatores
  observados/ausentes e justificativa de seleção;
- `workspace_snapshot.schema.json`: estado observável de um ciclo de seleção;
- `workspace_broadcast.schema.json`: payload do broadcast interno.

`source_refs` preserva a proveniência de `CanonicalEvent`, `Episode`, padrão
ou conteúdo interno. O workspace não interpreta ausência de um fator como
zero: fatores não observados aparecem em `missing_factors` e não participam da
média ponderada.

`selection_churn` mede a diferença simétrica entre os conjuntos ativo anterior
e atual, normalizada pela união. O primeiro snapshot com itens ativos pode ter
churn diferente de zero; um snapshot idêntico subsequente tem churn zero.

O broadcast é transportado localmente por um `CanonicalEvent` de tipo
`workspace.selection.v1`. Seu payload contém o snapshot já validado. Itens
ativos no snapshot devem validar individualmente contra
`workspace_item.schema.json`; o schema de snapshot mantém essa composição
leve para que não haja cópia manual da definição de item em cada linguagem.

Os scores são decisão operacional de priorização, não fatos, objetivos,
intenções ou evidência de consciência. O contrato não autoriza ações.
