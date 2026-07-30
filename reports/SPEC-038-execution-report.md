# Relatório de execução

SPEC: SPEC-038 — Promoção nativa do self-model funcional
Agente: Codex
Data: 2026-07-30

## Incremento

- Implementado VersionedFunctionalSelfModel C++ com snapshots imutáveis,
  hash-chain, replay por versão e proveniência de eventos.
- Implementadas decisões self_model_gate_v1 e baseline
  unconstrained_decision_v0, sem execução de ações ou planos.
- Implementado FunctionalSelfModelPlugin removível com CapabilityDescriptor.
- Adicionado runner JSON-lines, fixtures de desenvolvimento/holdout e
  manifesto de promoção.
- Preservado o contrato legado self_model.schema.json; os contratos
  complementares de evento, snapshot e decisão permanecem versionados.

## Evidência científica e operacional

A hipótese H6 foi operacionalizada por incompatibilidades de decisão,
explicabilidade, estabilidade e recuperação após remoção/reinstalação. No
desenvolvimento, o tratamento registrou 2 incompatibilidades em 12 decisões,
contra 9 do baseline sem consulta ao snapshot; no holdout, registrou 2 em
5 decisões. A taxa de explicabilidade e estabilidade foi 1,0 nos conjuntos.
Essas métricas são evidência operacional sobre fixtures congelados, não
ground truth nem evidência de estados mentais reais.

Equivalência Python/C++ e holdout passaram sem divergências. A medição nativa
mais recente apresentou p50 20,1801 ms, p95 21,93078 ms, máximo 54,5716 ms e
throughput 230,18 casos/s. Memória não foi medida pelo processo Python neste
ambiente e foi registrada como 0,0 MiB; isso não é uma alegação de consumo
nulo.

## Gates executados

- python tools/validate_functional_self_model_promotion.py — equivalência,
  holdout, baseline, ablação, replay e invariantes aprovados.
- CTest MinGW/vcpkg — functional_self_model aprovado.
- Python suite, lint, tipos, contratos, maturidade, SPECs e documentação —
  aprovados após a atualização final.

## Maturidade

O componente está reference_status: frozen, native_status: equivalent e
product_status: unavailable. Nenhuma aprovação humana foi inventada e o
manifesto não foi inserido no registro de promoções aprovadas.
