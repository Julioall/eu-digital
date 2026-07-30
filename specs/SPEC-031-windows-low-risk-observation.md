---
id: SPEC-031
title: Observação Windows de baixo risco
status: done
phase: beta
dependencies: [SPEC-030]
adrs: [ADR-0009, ADR-0026, ADR-0027]
contracts: [capture_policy.schema.json]
---

# SPEC-031 — Observação Windows de baixo risco

Status: done
Fase: Product Beta
Dependências: SPEC-030
ADRs aplicáveis: ADR-0009, ADR-0026, ADR-0027

## Objetivo

Conectar somente observação Windows de baixo risco à arquitetura de
capacidades. O produto deve observar executável/categoria e atividade
agregada de input somente quando consentido, sem publicar título de janela ou
clipboard por padrão.

## Requisitos

1. Processos e aplicação ativa publicam executável e categoria, não título
   bruto.
2. Título de janela e clipboard permanecem desabilitados por padrão.
3. Redação, allowlist e denylist são aplicadas antes da criação do evento.
4. A denylist inclui gerenciadores de senha, sessões privadas e aplicativos
   sensíveis obrigatórios.
5. Habilitação textual exige allowlist explícita e indicador separado no
   payload.
6. Pausa global e bloqueio por aplicativo não publicam evento e atualizam a
   health do sensor.
7. O sensor não trata ausência de observação como observação negativa.
8. A capacidade permanece removível, com `CapabilityDescriptor` e health
   independente.

## Escopo negativo

- não capturar OCR ou bytes de tela;
- não capturar texto de teclado;
- não persistir clipboard bruto;
- não observar gerenciadores de senha ou sessões privadas;
- não iniciar serviço ou auto-start;
- não executar ações;
- não alterar `CanonicalEvent` ou introduzir telemetria externa.

## Critérios de aceite

- [x] Fixture válida e inválida da política são validadas.
- [x] Título e clipboard default são suprimidos em testes nativos.
- [x] Allowlist, denylist, redator e indicador textual são testados.
- [x] Pausa global e health por sensor são testados.
- [x] Adaptador Windows não lê título quando a opção está desabilitada.
- [x] CTest Windows/Linux, suíte Python, validação híbrida e documentação
      passam.

## Protocolo operacional

Hipótese: retirar conteúdo textual por padrão e bloquear aplicativos sensíveis
reduz exposição de dados sem impedir métricas agregadas de atividade.

- baseline: adaptador atual que publica título e permite clipboard;
- métrica: eventos textuais suprimidos, eventos agregados preservados,
  bloqueios de denylist e falsos desbloqueios em fixtures;
- ablação: remover denylist, redator ou pausa global separadamente;
- falsificação: título bruto aparecer no evento default, clipboard ser emitido
  sem consentimento, aplicativo bloqueado gerar evento ou ausência virar zero.

Estas métricas são operacionais e não constituem evidência cognitiva.
