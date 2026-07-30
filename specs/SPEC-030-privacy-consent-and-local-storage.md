---
id: SPEC-030
title: Privacidade, consentimento e armazenamento local
status: done
phase: 0.3
dependencies: [SPEC-028, SPEC-029]
adrs: [ADR-0002, ADR-0009, ADR-0010, ADR-0024, ADR-0026]
contracts: [consent_policy.schema.json, storage_policy.schema.json, storage_health.schema.json, data_management_request.schema.json]
---

# SPEC-030 — Privacidade, consentimento e armazenamento local

Status: done
Fase: 0.3
Dependências: SPEC-028, SPEC-029
ADRs aplicáveis: ADR-0002, ADR-0009, ADR-0010, ADR-0024, ADR-0026

## Objetivo

Estabelecer a fronteira local de consentimento, proteção e governança dos
dados antes da integração de sensores reais. O runtime deve negar por padrão,
pausar captura explicitamente, preservar transações seguras sob quota e
permitir exportação, exclusão e recuperação solicitadas pelo usuário.

## Entregáveis

- contratos versionados de consentimento, política de armazenamento, saúde e
  solicitações de gestão de dados;
- ledger nativo de concessão/revogação por sensor e finalidade;
- pausa global sem transformar ausência em observação negativa;
- controller de quota com defaults versionados e modelo contabilizado à parte;
- DPAPI para proteção de snapshots de consentimento no Windows;
- exportação, exclusão confirmada e recuperação local com quarentena;
- referência Python para validação dos contratos e testes C++ nativos.

## Requisitos

1. O padrão de consentimento deve ser negar.
2. A decisão deve ser granular por sensor e finalidade, versionada e auditável.
3. Revogação deve bloquear novas capturas sem apagar automaticamente o
   histórico.
4. Pausa global deve bloquear novas capturas mesmo quando existe concessão.
5. A política deve usar defaults 30/365/14 dias e 10 GiB, versionados.
6. Banco, WAL, índices, quarentena, backups e payloads devem contar para a
   quota; modelos devem ser separados.
7. Overflow de quota deve suspender novas capturas, marcar `degraded` e pedir
   decisão; não pode apagar dados arbitrariamente.
8. Exportação e exclusão devem exigir confirmação explícita e escopo local.
9. Recuperação deve copiar backup conhecido, verificar a cópia e preservar o
   arquivo substituído em quarentena.
10. Snapshots de consentimento no Windows devem usar DPAPI; plataformas sem
    DPAPI devem reportar indisponibilidade sem fallback em texto puro.
11. O runtime deve expor uma `StorageHealth` separada sem quebrar o contrato
    `RuntimeHealth` 1.0 existente.

## Escopo negativo

- não integrar sensores Windows reais;
- não implementar OCR, título de janela ou clipboard;
- não escolher modelo, interface, avatar ou MSIX;
- não enviar dados para a nuvem;
- não apagar dados automaticamente por quota ou revogação;
- não alterar o schema público de `CanonicalEvent`;
- não promover mecanismo cognitivo.

## Critérios de aceite

- [x] Contratos válidos e inválidos são verificados por fixtures e referência
      Python.
- [x] Consentimento, revogação, negação padrão e pausa global possuem testes
      nativos e de contrato.
- [x] Defaults e buckets da quota são versionados e testados.
- [x] Transação que excede quota não apaga dados e deixa health degradado.
- [x] Exportação e exclusão exigem confirmação e não escapam da raiz local.
- [x] Recuperação preserva o arquivo corrompido e verifica o backup.
- [x] DPAPI possui implementação Windows e indisponibilidade explícita fora
      do Windows.
- [x] Runtime expõe `StorageHealth` sem alterar `RuntimeHealth` 1.0.
- [x] CTest, suíte Python, validação híbrida e documentação passam.

## Protocolo operacional

Hipótese: consentimento granular e overflow explícito reduzem captura não
autorizada e perda arbitrária de dados sem exigir serviços externos.

- baseline: runtime com timeline e sem ledger/quota de captura;
- métricas: decisões sem consentimento bloqueadas, revogações efetivas,
  pausas observáveis, transações abortadas sem perda, recuperações verificadas
  e operações confirmadas;
- ablação: remover a pausa global e a guarda de quota;
- falsificação: uma captura ser aceita sem concessão, quota causar deleção
  automática, recuperação perder o original ou DPAPI ser substituída por
  armazenamento plaintext.

Estas métricas são operacionais e não constituem evidência cognitiva.

## Dependências e bloqueios

A SPEC reutiliza `CanonicalEvent`, `TimelineStore` e o ciclo de vida nativo.
Sensores concretos só poderão consumir o ledger depois da SPEC-031/032.
O smoke test Windows da SPEC-028 permanece um gate independente; a
implementação DPAPI desta SPEC será validada quando o toolchain nativo for
executado.
