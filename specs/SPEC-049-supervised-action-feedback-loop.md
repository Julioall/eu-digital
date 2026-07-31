---
id: SPEC-049
title: Supervised Action Feedback Loop
status: draft
created: 2026-07-31
authors: [Julio]
---

# SPEC-049: Supervised Action Feedback Loop

## 1. Motivação
A agência do Eu Digital exige que ações sistêmicas (como acionar o modo foco, fechar um programa ou agendar um evento) sejam supervisionadas. Já temos a infraestrutura de orquestração, porém o fechamento do loop (sugerir -> usuário aceita -> executar -> observar resultado -> atualizar memória) precisa ser solidificado como um caminho formal no runtime.

## 2. Objetivo
Criar a fundação no `RuntimeHost` para despachar ações de sistema isoladas e seguras, aguardar a permissão do usuário via interface (Tray Menu/Avatar), executar o binário correspondente à capacidade (via `CapabilityRegistry`), capturar a saída (sucesso/falha) e re-injetá-la no `EventBus` como `action_result`.

## 3. Escopo Positivo
- Definir o formato de evento de resposta `action_result`.
- Criar a ponte de comunicação assíncrona entre o Shell (Interface Gráfica que coleta o "Sim" do usuário) e o `CapabilityRegistry` que de fato invoca a ação no SO.
- Re-injetar o resultado no Event Bus, permitindo que a `EpisodicMemory` armazene se a ação deu certo ou não.

## 4. Escopo Negativo
- Nenhuma ação não supervisionada (zeroshot) deve ser permitida nesta fase.
- Não implementar drivers pesados de automação de UI (Mouse/Teclado). Ações devem ser via chamadas nativas isoladas (APIs do Windows/Scripts).

## 5. Aceite Mensurável
- Após uma ação aprovada e executada que gera um erro (ex: falha de permissão do Windows), o evento de `action_result` volta ao EventBus com status "failed".
- Na iteração seguinte, o `SelfModel` recebe o pacote de que "a habilidade X falhou no passado", adaptando sua confiança para usos futuros (degradação graciosa).
