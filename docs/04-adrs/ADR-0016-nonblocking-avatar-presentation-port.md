# ADR-0016 — Porta de apresentação de avatar não bloqueante

Status: aceito
Data: 2026-07-29
Decisores: aprovação humana explícita

## Contexto

A SPEC-014 requer presença visual discreta, perguntas contextuais e controles
de correção, adiamento e silêncio. Não há escolha aprovada para Tauri, Qt ou
outro host desktop. Acoplar o núcleo a uma janela concreta ou apresentar humor,
emoção ou intenção criaria dependência e antropomorfismo indevidos.

## Decisão

Criar uma referência local de apresentação por porta injetada.

- O estado visual é limitado a `hidden`, `quiet`, `notice` e `question`, com
  propriedades explícitas de não bloqueio (sem foco, sem captura de teclado e
  sem interceptação de ponteiro).
- Avisos referenciam hipótese, confiança, evidência e motivo; o controller não
  inventa conteúdo nem consulta um LLM.
- As únicas respostas do usuário são `correct`, `defer` e `silence`, todas
  estruturadas e auditáveis. Entrega de mensagens, ações e persistência estão
  fora da SPEC.
- O host desktop é um adaptador opcional. A referência Python valida estado e
  interação sem escolher framework ou abrir uma janela durante testes.

## Consequências

- a interface pode ser avaliada quanto a interrupção sem declarar estados
  internos humanos;
- o host pode ser substituído sem alterar diálogo, memória ou cognição;
- uma superfície ausente permanece uma capacidade explícita, não falha do
  núcleo.

## Limites e reversão

O avatar não prova presença, emoção ou consciência. Remover o presenter ou
selecionar `hidden` interrompe a superfície sem apagar perguntas ou feedback.
