# ADR-0013 — Metacognição calibrada e curiosidade com orçamento local

Status: aceito
Data: 2026-07-28
Decisores: aprovação humana explícita

## Contexto

A SPEC-011 precisa avaliar hipóteses e decidir entre formular uma pergunta
estruturada ou permanecer em silêncio. O contrato textual de hipótese não era
executável, e não havia semântica para calibração, feedback, ganho
informacional, interrupções ou redundância.

A evidência para metacognição separada é A/B e para curiosidade por ganho é B.
Ela justifica uma referência local, mas não permite usar confiança verbal de
LLM como calibração, nem alegar compreensão, intenção ou consciência.

## Opções consideradas

1. Perguntar sempre que houver incerteza.
2. Aceitar a confiança declarada por um LLM ou prompt.
3. Referência local, estruturada e calibrável por resultados verificados.
4. Busca externa, diálogo autônomo ou ação para resolver incerteza.

## Decisão

Adotar a opção 3 para a SPEC-011.

- Hipóteses, avaliações, perguntas e respostas possuem schemas executáveis e
  versionados, com proveniência, evidência favorável/contrária e alternativas.
- `evidence_ratio_v1` forma a confiança bruta a partir de confiança declarada
  e balanço de evidências observadas. Ausência de evidência não é prova
  contrária.
- `bucketed_beta_v1` ajusta confiança apenas após resultados locais
  confirmados/rejeitados. Brier, ECE, AUROC e risk–coverage distinguem
  calibração de desempenho operacional.
- `information_gain_v1` estima ganho por entropia de incerteza e resolução
  esperada declarada. `fixed_gain_v0` e `raw_confidence_v0` são controles
  selecionáveis pela mesma configuração para ablação.
- Perguntas são objetos estruturados; esta SPEC não envia mensagens, não faz
  busca externa, não executa ações e não requer LLM. Orçamento móvel,
  cooldown, redundância e correções podem produzir supressão ou silêncio.
- A referência permanece em Python. Promoção C++ exige SPEC-026, equivalência
  e validação independente conforme ADR-0010 e ADR-0011.

## Consequências

Positivas:

- cada pergunta aponta para uma hipótese e uma estimativa auditável de ganho;
- feedback pode reduzir confiança e repetição sem apagar proveniência;
- ablações de calibração, ganho e supressão usam a mesma interface;
- a ausência de resposta fica explícita como inconclusiva, não negativa.

Custos e limites:

- métricas só ganham validade científica contra outcomes/holdout congelados;
- uma pergunta estruturada não é diálogo nem evidência de curiosidade humana;
- o orçamento é uma política técnica inicial, não uma preferência do usuário;
- não há persistência longitudinal, self-model, planejamento ou ação nesta
  SPEC.

## Plano de reversão

Selecionar `raw_confidence_v0` e `fixed_gain_v0`, ou retirar o chamador do
módulo no experimento. Como o módulo não altera hipótese, episódio, padrão ou
evento de origem, sua remoção preserva seus registros e contratos.
