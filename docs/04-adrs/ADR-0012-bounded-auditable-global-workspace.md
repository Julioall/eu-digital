# ADR-0012 — Workspace global limitado e auditável

Status: aceito
Data: 2026-07-28
Decisores: aprovação humana explícita

## Contexto

A SPEC-010 requer competição por atenção, capacidade limitada, expiração,
justificativa de seleção, broadcast interno e estado observável. O modelo
cognitivo lista fatores possíveis de saliência, mas não define uma semântica
executável para itens, ausência de observação, desempate, ciclo de vida ou
evidência científica.

A matriz de evidência classifica workspace limitado como evidência B, com
capacidade e competição obrigatoriamente avaliadas por ablação. Isto autoriza
uma referência de laboratório, não uma alegação de consciência nem promoção
automática ao runtime C++.

## Opções consideradas

1. Contexto ilimitado, sem competição explícita.
2. Regras fixas por tipo de sensor ou catálogo de tarefas.
3. Referência local, limitada e determinística com sinais observados.
4. Saliência aprendida ou baseada em modelo pesado nesta fase.

## Decisão

Adotar a opção 3 para a SPEC-010.

- O workspace aceita candidatos genéricos com referências de origem; não
  importa sensores, ferramentas, atuadores ou modelos concretos.
- A política `observed_weighted_mean_v1` calcula a saliência como média
  ponderada apenas dos fatores efetivamente observados. Fator ausente é
  registrado e não equivale a valor zero nem a evidência negativa.
- A capacidade é configurável e a ordenação é determinística: maior score e,
  em empate, menor `candidate_id`.
- Itens recebem TTL local, expiram de forma explícita e não são persistidos
  nesta primeira referência de curta duração. Snapshots serializáveis
  permitem auditoria e replay determinístico quando o relógio é fornecido.
- O broadcast interno usa um `CanonicalEvent` local com payload versionado
  `workspace.selection.v1`; nenhum dado é enviado para fora da máquina.
- O controle `fifo_capacity_v0` é selecionável na mesma configuração e a
  ablação remove fatores da política sem reescrever o módulo. A hipótese H3 é
  avaliada por precisão/recall de seleção contra relevância anotada, custo de
  capacidade, churn e robustez a ruído.
- O mecanismo permanece em Python. Uma implementação C++ requer o pipeline
  de promoção, equivalência e evidência independente definidos pelas
  ADR-0010, ADR-0011 e SPEC-026.

## Consequências

Positivas:

- seleção e exclusão são auditáveis por snapshot;
- o experimento pode remover fatores e variar capacidade;
- o núcleo preserva independência de capacidades e observabilidade parcial;
- o contrato permite futura porta sem copiar schemas entre linguagens.

Custos e limites:

- score não é uma crença, intenção, fato ou medida de consciência;
- pesos iniciais são baseline configurável, não resultado aprendido;
- relevância anotada e holdout ainda são necessários para validade científica;
- não há persistência de longo prazo, ação, diálogo, planejamento ou C++ nesta
  SPEC.

## Plano de reversão

Selecionar `fifo_capacity_v0` na configuração para a ablação, ou retirar o
chamador do workspace do experimento. Como o workspace não altera eventos,
episódios ou padrões de origem, sua remoção preserva seus contratos e
histórico.
