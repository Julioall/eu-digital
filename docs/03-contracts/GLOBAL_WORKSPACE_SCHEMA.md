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

## Promoção nativa SPEC-037

O candidato C++ recebe os mesmos JSON-lines da referência Python pelo runner
promotion_fixture_runner --global-workspace. Cada operação produz um snapshot
determinístico; broadcast transporta o último snapshot como evento local
workspace.selection.v1. A referência, o conjunto de desenvolvimento e o
holdout bloqueado estão registrados em
promotions/cognition.global_workspace.v1.json.

O componente é removível e permanece product_status: unavailable sem revisão
humana. Equivalência entre linguagens, holdout, baseline FIFO, ablação,
capacidade, expiração e latência são verificações operacionais; não
constituem ground truth, evidência cognitiva ou autorização de ação.
