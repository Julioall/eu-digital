---
id: SPEC-046
title: Cognitive State Persistence and Recovery
status: draft
created: 2026-07-31
authors: [Julio]
---

# SPEC-046: Cognitive State Persistence and Recovery

## 1. Motivação
A memória episódica, o modelo de mundo e o self-model atualmente existem em estado volátil ou gravam entradas assíncronas no banco de dados. No entanto, se o sistema for reiniciado ou atualizar sua versão (fechar e abrir o processo), o "contexto de curto prazo" (como a surpresa atual, os buffers da área de trabalho global e os temporizadores do `SuggestionOrchestrator`) são perdidos. Precisamos de continuidade operacional.

## 2. Objetivo
Garantir que os estados do `GlobalWorkspace`, orçamentos do `SuggestionOrchestrator`, e a fronteira ativa do `EpisodeSegmenter` sobrevivam a reinicializações, encerramentos inesperados e trocas de sessão de usuário, utilizando a fundação do SQLite (`PrivacyStorage`).

## 3. Escopo

### 3.1 Escopo Positivo
- Implementar `CheckpointService` no C++.
- Gravar um Snapshot do estado ativo (JSON binário ou BSON) no SQLite a cada `X` eventos ou a cada encerramento limpo.
- Restaurar esse estado na subida do `RuntimeHost` (`--run`).
- Tratar incompatibilidade de versões (ex: Snapshot da versão v1.0, e o executável atual é v1.1).

### 3.2 Escopo Negativo
- Não criar persistência na nuvem.
- Não manter histórico eterno de checkpoints (apenas o último estado seguro).
- Não gravar áudio, imagens brutas ou hashes PII no checkpoint (apenas o vetor cognitivo).

## 4. Dependências e Contratos
- Depende: `RuntimeHost`, `PrivacyStorage`, `CognitiveCoordinator`.
- Contratos Envolvidos: Novo schema `cognitive_snapshot.schema.json`.

## 5. Falhas e Tratamento
Se um Snapshot falhar ao ser carregado (corrompido, I/O error ou schema version mismatch), o sistema deve descartá-lo (amnésia de curto prazo) e inicializar um estado vazio, emitindo um log explícito de "Cold Start". O Produto não pode crashar por causa de cache inválido.

## 6. Aceite Mensurável
- Após receber um evento de "Janela Focada" e um de "Teclado", derrubar o processo (`SIGKILL`). Ao religar, o `EpisodeSegmenter` deve estar no meio do episódio correto, e não abrir um novo do zero.
- Tamanho do Snapshot no disco não pode ultrapassar 1 MB.
