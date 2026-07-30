# ADR-0029: promoção atômica da segmentação de episódios

- Status: accepted
- Data: 2026-07-29
- Decisores: governança do projeto

## Contexto

A segmentação existe como referência Python em `SPEC-007`, mas o runtime
instalado usa C++. A marcação `done` da SPEC de laboratório não autoriza que um
mecanismo seja carregado no produto. A promoção precisa preservar o contrato,
ser removível e deixar explícita a diferença entre equivalência computacional e
validade científica.

## Decisão

Promover a segmentação como uma unidade atômica, com `promotion_id` próprio,
manifesto, fixture congelada, holdout separado e `CapabilityDescriptor` nativo.
O candidato C++ deve reproduzir a referência sem fallback semântico e sem
interpretar contexto ausente como mudança. O registro de promoções permanece
inalterado até uma revisão humana fornecer `approval_review_id`.

O componente pode ser classificado como `native_status: equivalent` e
`product_status: unavailable` após os gates automatizados. A classificação
`promoted` só é válida com uma aprovação registrada.

## Consequências

- Memória, padrões e demais mecanismos continuam independentes e não são
  implicitamente promovidos.
- A referência Python é uma implementação congelada/regressiva, não um oráculo
  científico.
- Divergências bloqueiam o gate; não são escondidas por tolerâncias ou regras.
- A capacidade pode ser removida pelo lifecycle de plugins sem alterar o núcleo.
- A execução local produz evidência auditável, mas não declara utilidade cognitiva.

## Alternativas rejeitadas

- Promover todas as capacidades cognitivas juntas: mistura hipóteses e impede
  localizar regressões.
- Inserir diretamente no runtime/produto: violaria a separação entre candidato,
  promoção aprovada e disponibilidade.
- Usar a saída Python como ground truth: contraria a política científica.
