# SPEC-004 — Sensor de interação

Status: blocked
Fase: 1
Dependências: SPEC-002, SPEC-023

## Objetivo
Registrar eventos de teclado, mouse, atalhos, clipboard e atividade de entrada.

## Requisitos
- Suporte a eventos brutos e agregados configurável.
- Associação com janela ativa.
- Taxa de digitação, pausas e atalhos.
- Payload versionado.

## Escopo negativo
Interpretação de intenção e execução de entrada.

## Critérios de aceite
- [ ] Eventos possuem contexto de janela.
- [ ] Alto volume é agregado sem perda de métricas.
- [ ] Clipboard produz evento separado.
