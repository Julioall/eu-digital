# ADR-0015 — Gateway local de modelo por porta e worker pesado único

Status: aceito
Data: 2026-07-29
Decisores: aprovação humana explícita

## Contexto

A SPEC-013 requer carregamento e invocação local sob demanda, prioridade,
cancelamento, timeout, descarregamento, templates versionados e saída
estruturada. Não há modelo multimodal definitivo, hardware confirmado ou ADR
que autorize escolher, baixar ou incorporar ONNX, OpenVINO, llama.cpp ou uma
API. ADR-0003 exige somente um modelo pesado carregado ou em inferência.

## Decisão

Implementar uma referência Python local que dependa apenas de uma porta
`LocalModelBackend` injetada.

- O gateway tem worker único e fila de prioridade estável; portanto nunca
  inicia duas inferências pesadas simultaneamente.
- Backends são selecionados por configuração local e podem ser trocados quando
  não há trabalho ativo. O núcleo não importa implementação concreta, modelo,
  biblioteca de inferência ou API remota.
- Cada requisição referencia um template local versionado e a resposta precisa
  satisfazer contrato estruturado antes de ser devolvida.
- Timeout tenta cancelar no backend e sempre descarrega o modelo ativo antes
  de liberar o worker. Cancelamento explícito trata itens em fila ou encaminha
  o pedido ao backend ativo.
- O backend determinístico usado nos testes é uma fixture, não um modelo nem
  um fallback de produção. A escolha do modelo multimodal continua a questão
  aberta 5.

## Consequências

Positivas:

- a política de um modelo pesado é verificável sem instalar runtime externo;
- a troca de backend preserva a interface e os contratos;
- recursos são liberados em erro, timeout, cancelamento e fechamento;
- templates e respostas são auditáveis e sem geração livre no gateway.

Custos e limites:

- o gateway não fornece inteligência sem um backend local compatível;
- métricas são operacionais (fila, timeout, cancelamento, descarregamento),
  não evidência de capacidade cognitiva;
- qualquer backend/modelo concreto, benchmark de qualidade ou promoção C++
  requer ADR próprio, SPEC-026 e validação independente.

## Plano de reversão

Remover o chamador ou desregistrar o backend local. Como o gateway não grava
memória, não executa ações e não muda contratos canônicos, a remoção não afeta
os demais módulos.
