# ADR-0020 — Consolidação com proveniência e retenção reversível

Status: aceito
Data: 2026-07-29

## Contexto

A SPEC-020 estava bloqueada por não definir como replay produziria
conhecimento sem apagar episódios, nem como retenção poderia ser avaliada sem
perder a fonte. A memória episódica existente é conservadora e não generaliza.

## Decisão

Adicionar uma referência Python local com três estruturas versionadas:

- SemanticKnowledge, sempre ligado a um ou mais episode_ids;
- ConsolidationRecord, que registra replay, política, custo e fontes;
- RetentionDecision, que distingue retain, archive e restore.

O replay consolidará somente chaves observáveis de contexto dos episódios
aprovados. Não transformará hipóteses em fatos e não usará LLM. Evidências
convergentes incrementam support_count e version; divergências permanecem em
contradictions/alternatives.

A política replay_with_provenance_v1 usa no máximo max_active_episodes e move
episódios excedentes para arquivo reversível. Nenhum episódio é apagado. O
baseline no_replay_v0 mantém a mesma interface sem gerar SemanticKnowledge.

## Protocolo científico

- hipótese H8: replay e consolidação reduzem esquecimento e contradições;
- métricas: retention_score, support/proveniência e taxa de contradição;
- ablação: no_replay_v0 pela mesma interface;
- falsificação: não há benefício de retenção ou aumenta a taxa de
  contradições/memórias sem fonte;
- custo de replay e retenção é registrado.

## Consequências

Positivas:

- conhecimento consolidado nunca perde suas fontes;
- retenção pode ser desfeita em teste;
- baseline e tratamento são comparáveis;
- arquivos históricos continuam recuperáveis.

Custos:

- conhecimento é limitado a generalizações observáveis de contexto;
- retenção não reduz armazenamento físico nesta fase;
- validade semântica exige avaliação posterior.

## Reversão

Desabilitar o consolidator preserva a memória episódica e retorna ao baseline.
Decisões de archive podem ser revertidas por restore sem alterar episódios.
