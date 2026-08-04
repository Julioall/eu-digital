# Validação operacional da orquestração da SPEC-045

Status: validação de engenharia concluída em 2026-08-04  
Alegação máxima: segurança, determinismo contratual e degradação previsível da
orquestração; não constitui evidência de aprendizagem ou cognição.

## Hipótese

`H-SPEC045-ORCHESTRATION`: uma fila limitada e um pipeline por portas
versionadas preservam proveniência e produzem um único resultado determinístico
sem reentrada, mesmo quando capacidades opcionais estão ausentes ou falham.

## Baseline

`pass_through_audit_v0`: validar a entrada e produzir silêncio auditável sem
capacidade cognitiva opcional registrada. O resultado esperado é `degraded`,
com etapas indisponíveis explícitas, sem decisão e sem dados inventados.

## Métricas e limiares congelados

- mediana do agendamento de entrada na fila menor ou igual a 1 ms;
- profundidade da fila nunca superior a `max_queue_size`;
- no máximo um resultado terminal por `event_id` e versão da política;
- zero reentradas de `cognitive.cycle.result`;
- falha ou timeout de predição não encerra o processo;
- replay não publica efeito externo.

O benchmark local de 4.000 entradas mediu mediana de 7.900 ns e p95 de
9.300 ns para limite de 1.000.000 ns. Esse número é evidência operacional desta
execução, não um resultado cognitivo.

## Ablação

O teste de baseline executa o coordenador com `CapabilityRegistry` vazio. As
demais fixtures permitem remover uma porta sem alterar o núcleo. A ausência é
registrada como etapa omitida; valores zero, hipóteses negativas e memórias
artificiais não são usados como substitutos.

A integração adicional substitui apenas boundary de episódio e predição por
dois adapters reais, mantendo as demais portas mockadas. Isso verifica que a
resolução ocorre por operação e prioridade, sem include concreto no
coordenador.

## Falsificação

A hipótese deve ser rejeitada se qualquer execução reproduzível:

- gerar dois resultados terminais para a mesma entrada;
- ultrapassar o limite da fila;
- reenviar um resultado interno ao pipeline;
- transformar ausência em observação negativa;
- bloquear shutdown além da quota por timeout cooperativo;
- derrubar o processo por falha isolada de adapter;
- publicar decisão ou efeito em replay;
- ultrapassar 1 ms de mediana no protocolo do benchmark.

## Verificação e limitações

Foram verificados contratos C++, sequência exata, backpressure, duplicata,
reentrada, timeout cooperativo, saída contratualmente inválida, falha de
predição, baseline vazio, dois adapters reais, integração com `RuntimeHost` e
flag de rollback.

Não foram avaliadas generalização computacional, validade científica, validade
ecológica, utilidade longitudinal ou melhoria do modelo do usuário. Essas
alegações exigem os protocolos e holdouts das funções cognitivas promovidas.
