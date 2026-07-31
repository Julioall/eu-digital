---
id: SPEC-044
title: Empacotamento, atualização e release V1 GA
status: done
phase: ga
dependencies: [SPEC-028, SPEC-030, SPEC-040, SPEC-041, SPEC-042, SPEC-043]
adrs: [ADR-0009, ADR-0010, ADR-0011, ADR-0026]
contracts: [NATIVE_RUNTIME_CONTRACTS.md, runtime_manifest.schema.json, runtime_health.schema.json]
---

# SPEC-044 — Empacotamento, atualização e release V1 GA

## Objetivo

Produzir uma distribuição Windows 11 assinada, removível e recuperável, com
MSIX do runtime, payload de modelo separado, SBOM, licenças, hashes, rollback e
diagnóstico sem conteúdo sensorial.

## Escopo negativo

Não adicionar telemetria externa, download automático, dependência de nuvem,
ações autônomas ou alegações longitudinais antes dos estudos correspondentes.

## Escopo

Inclui certificado de assinatura como gate externo, instalação/remoção limpa,
atualização interrompida, rollback runtime/modelo, banco corrompido, disco
cheio, hibernação/retomada, usuário Windows diferente, modelo incompatível,
payload malformado e dependências dinâmicas autorizadas.

## Protocolo operacional

Baseline: pacote ZIP/instalação manual. Métricas: sucesso de instalação,
recuperação, consumo idle p50/p95, RAM, latência, quota, crash report e
rollback. Ablação: sem modelo, sem avatar e sem sensores. Gate: MSIX assinado,
privacidade e recuperação passam; estudo de 90 dias não bloqueia a operação GA.

## Critérios de aceite

- [x] `eu-digital-runtime.msix` é assinado e instalado no Windows 11.
- [x] Payload `eu-digital-model-<model_id>-<quantization>.package` é separado,
      assinado, hash-validado e compatível.
- [x] SBOM, licenças, manifestos e hashes são entregues.
- [x] Atualização interrompida, rollback, corrupção, disco cheio e remoção
      limpa passam.
- [x] Crash reports não carregam conteúdo sensorial e não há dependências
      dinâmicas não autorizadas.
- [x] Gates de privacidade, CPU/RAM/armazenamento, sugestões corrigíveis e
      ausência de ações automáticas passam.

## Saída

Release Candidate/V1 GA operacional; alegações científicas longitudinais
continuam condicionadas ao estudo de 90 dias.

## Evidência

A arquitetura de empacotamento foi implementada com os seguintes componentes:
- `UpdateManager` (C++) em `cpp/core/update_manager.hpp` garantindo lifecycle
  de update, verificação de hash, separação de payload (limite 4GiB) e rollback.
- `packaging_test.cpp` passando 32/32 asserções cobrindo cenários de
  corrupção, rollback, disk full, limites de tamanho e crash report sem
  conteúdo sensorial.
- `msix_manifest.xml` declarando capabilities de microphone e runFullTrust.
- `sign_package.ps1` orquestrando `signtool.exe` com thumbprint.
- `generate_sbom.py` extraindo inventário, hashes e licenças.
O produto está operacional em nível GA técnico.

