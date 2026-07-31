---
id: SPEC-050
title: Minimum Usable Application
status: draft
created: 2026-07-31
authors: [Julio]
---

# SPEC-050: Minimum Usable Application

## 1. Motivação
Todas as implementações das SPECs 045 a 049 criam a fundação funcional do "Ciclo Cognitivo". Contudo, este ciclo ainda será apenas um executável de linha de comando sem as janelas, e o shell Qt (criado na SPEC-042) ainda roda desvinculado do cérebro. Para termos um produto de verdade (Vertical Slice), é preciso unir o backend em C++ com o frontend Qt e o gerenciador de pacotes, entregando um único processo orquestrador no desktop do usuário.

## 2. Objetivo
Unir as instâncias do `QtAvatarShell` com a engine do `RuntimeHost` (e seu `CognitiveCoordinator`). O aplicativo deve iniciar oculto na bandeja do sistema, levantar o runtime cognitivo em uma thread dedicada de background, injetar as saídas do `DecisionOutputRouter` diretamente nos slots Qt do Avatar, e prover uma saída limpa (graceful shutdown) quando encerrado pelo usuário via Tray Icon.

## 3. Escopo Positivo
- Integrar a build CMake para linkar o executável Qt contra as bibliotecas estáticas `core` do Eu Digital.
- Escrever `cpp/app/eu_digital_desktop.cpp` como o entrypoint oficial da aplicação de usuário final.
- Subir duas threads principais: Thread 1 (Qt Event Loop e GUI), Thread 2 (Cognitive Pipeline e EventBus).
- Mapear os cliques do usuário nos diálogos do Avatar para "eventos canônicos" (InputInteractionSensor) enviados ao EventBus (Thread 2).

## 4. Escopo Negativo
- Não reescrever shaders ou partes visuais do Qt.
- Não introduzir bloqueios (mutex lock) que façam a janela travar (UI Thread Hang) enquanto o pipeline cognitivo processa ou carrega um modelo.

## 5. Aceite Mensurável
- Iniciar o `eu_digital_desktop.exe` e observar que o uso de RAM (sem o LLM carregado) estabiliza. A janela não pode apresentar travamentos (60 fps fixos) independentemente da carga computacional na thread cognitiva.
- O clique no ícone da bandeja deve emitir um evento que seja processado pelo ciclo cognitivo, provando comunicação full-duplex entre GUI e Core.
