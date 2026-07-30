# ADR-0031: promoção atômica da aprendizagem de padrões

- Status: accepted
- Data: 2026-07-29
- Decisores: governança do projeto

## Contexto

O laboratório já possui um learner incremental que agrupa observações
numéricas, aplica suporte mínimo, recebe feedback e cria versões em drift. A
concordância com Python não é prova de que os padrões são verdadeiros ou úteis.

## Decisão

Promover somente `cognition.pattern_learning.v1`, com contrato de padrão,
fixtures congeladas, baseline de chave exata, ablação, holdout e plugin C++
removível. O estado `promoted` do padrão é apenas uma condição operacional de
suporte/confiança; não autoriza nomeação, ação ou planejamento.

O registro de promoções exige revisão humana e `approval_review_id`. Até lá, a
maturidade nativa pode ser `equivalent`, mas a disponibilidade do produto fica
`unavailable`.

## Consequências

- Drift e feedback permanecem auditáveis e não apagam observações.
- Features ausentes não são convertidas em zero; a distância usa apenas chaves
  compartilhadas.
- A próxima SPEC de world model recebe padrões como entrada promovida, mas não
  é antecipada aqui.

## Alternativas rejeitadas

- Nomear automaticamente clusters: excede a evidência observada.
- Promover padrões com world model: mistura mecanismos e gates científicos.
- Usar um LLM para escolher clusters: viola a independência do núcleo.
