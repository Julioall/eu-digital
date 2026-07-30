# Relatório de execução

SPEC: SPEC-037 — Promoção nativa do workspace global  
Agente: Codex  
Data: 2026-07-30

## Incremento

- Implementado GlobalWorkspace C++ com seleção ponderada, baseline FIFO,
  empates determinísticos, prioridade explícita, TTL, limite de candidatos,
  churn e proveniência.
- Adicionado WorkspaceSnapshot, WorkspaceBroadcast e GlobalWorkspacePlugin
  com CapabilityDescriptor removível.
- Adicionado runner JSON-lines promotion_fixture_runner --global-workspace,
  fixtures de desenvolvimento/holdout e manifesto de promoção.
- Mantida a separação entre saliência operacional e fatos, planos, tarefas,
  intenção, diálogo, avatar e ação.

## Evidência científica e operacional

A hipótese H3 usa precisão/recall/F1 de seleção anotada, churn, ocupação,
baseline FIFO e ablação de capacidade reduzida. No desenvolvimento, o
tratamento obteve F1 médio 0,80 contra 0,30 do baseline FIFO; no holdout, F1
1,00. Esses números são métricas operacionais sobre anotações congeladas, não
ground truth nem evidência cognitiva.

Equivalência Python/C++ e holdout passaram com zero divergências. A medição
MinGW mais recente apresentou p50 14,5802 ms, p95 15,7255 ms, máximo
17,7543 ms e throughput 339,11 casos/s. Memória não foi medida pelo processo
Python neste ambiente e foi registrada como 0,0 MiB; o limite de memória
permanece um gate declarado, não uma alegação de consumo nulo.

## Gates executados

- python -m unittest discover -s python/tests — 192/192.
- uvx --from ruff ruff check — aprovado.
- uvx --from mypy mypy python/eu_digital_lab — 27 módulos sem erros.
- CTest MinGW/vcpkg — 19/19.
- CTest MSVC/vcpkg — global_workspace e world_model aprovados.
- python tools/validate_workspace_promotion.py — equivalência, holdout,
  invariantes, baseline, ablação e performance aprovados.
- Validadores de contratos, maturidade, SPECs e documentação — aprovados.

O CTest MSVC completo apresentou timeout em testes legados do runtime/SQLite,
enquanto a suíte completa equivalente MinGW passou; isso não afetou o alvo
novo nem a validação da SPEC-037 e fica registrado para investigação separada.

## Maturidade

O componente está reference_status: frozen, native_status: equivalent e
product_status: unavailable. Nenhuma aprovação humana foi inventada e o
manifesto não foi inserido no registro de promoções aprovadas.
