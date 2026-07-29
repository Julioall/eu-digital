# ADR-0021 — World model incremental e erro preditivo

Status: aceito
Data: 2026-07-29

## Contexto

A SPEC-021 exige previsão explícita de próximos estados, comparação com
baseline, medição de erro e uma resposta auditável de novidade. O projeto já
possui eventos, episódios, padrões e workspace, mas não possui um contrato para
uma distribuição preditiva nem uma política para drift. A função é de pesquisa
no laboratório Python; promovê-la ao runtime C++ exigiria evidência e um
manifesto posterior.

## Decisão

Adicionar um world model local, determinístico e incremental com três políticas
comparáveis:

- `frequency_baseline_v0`: distribuição marginal dos estados observados;
- `markov_order1_v0`: baseline de transição de primeira ordem;
- `incremental_markov_v1`: modelo de n-grama incremental, com contexto máximo
  configurável e fallback para contextos menores e para a distribuição global.

Cada previsão publica a distribuição completa de próximos estados conhecidos,
e cada resultado observado registra log loss, top-k, confiança, contribuição de
saliência e proveniência. Erro alto gera um `DriftSignal` quando a média da
janela excede o limiar configurado. O sinal reduz a confiança, limpa as
transições aprendidas e inicia reaprendizagem a partir das observações
seguintes. Nenhum estado é inventado por ausência de observação.

A integração com saliência é por um fator observado `prediction_error`,
limitado a `[0, 1]`, calculado de forma monotônica a partir do log loss e
referenciado no contrato. O workspace continua responsável pela política final
de seleção.

## Protocolo científico

- hipótese H9: previsão explícita de próximos estados melhora detecção de
  novidade;
- baseline: `frequency_baseline_v0` e `markov_order1_v0`;
- métrica: log loss, top-k, detecção de mudança e ganho de pergunta;
- ablação: executar a mesma sequência com o baseline de frequência, sem
  contexto de transição;
- falsificação: clusters sem previsão têm resultado equivalente no holdout;
- validade: os testes unitários verificam o mecanismo, mas não constituem
  evidência cognitiva ou ecológica.

## Consequências

Positivas:

- distribuição e surpresa podem ser auditadas sem LLM;
- drift reduz confiança e inicia reaprendizagem de forma reversível;
- baseline e tratamento usam a mesma interface e podem ser avaliados em
  holdout separado.

Custos:

- o modelo representa apenas estados discretos observados;
- o estado de aprendizagem é mantido em memória nesta referência;
- o sinal de saliência é evidência operacional, não prova de novidade
  cognitiva.

## Reversão

Desabilitar o predictor retorna ao baseline de frequência e não altera eventos,
episódios, padrões ou memória. Limpar o estado do predictor remove somente
contagens derivadas e preserva as observações de origem.
