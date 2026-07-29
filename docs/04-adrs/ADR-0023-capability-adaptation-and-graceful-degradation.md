# ADR-0023 — Adaptação funcional e degradação graciosa

Status: aceito
Data: 2026-07-29

## Contexto

A SPEC-023 registra capacidades removíveis e o self-model operacional, mas não
define como atenção, confiança, previsões e planos reagem à observabilidade
parcial. A ausência de visão, áudio ou atuadores não pode interromper o núcleo,
inventar dados ou apagar memória.

## Decisão

Adicionar uma referência Python independente de adaptadores concretos. Ela
recebe somente eventos de capacidade e mantém três artefatos versionados:

- `CapabilityAdaptationEvent`, com limitações, hipóteses afetadas, predições
  invalidadas e planos bloqueados;
- `ObservabilityProfile`, com modalidades disponíveis, cegueiras conhecidas,
  pesos de atenção redistribuídos e ajustes de confiança;
- `CapabilityOnboarding`, que mantém uma modalidade em `calibrating` até atingir
  amostras mínimas antes de permitir influência estável.

O tratamento normaliza a atenção entre modalidades disponíveis, reduz a
confiança de hipóteses que dependem da modalidade ausente e invalida somente
predições explicitamente dependentes dela. Planos que exigem uma capacidade
indisponível ficam bloqueados. Uma implementação equivalente pode substituir a
anterior sem alterar `agent_id`, geração de identidade ou histórico.

## Protocolo científico

- hipótese H13/H14: adaptação à plasticidade de capacidades preserva
  integridade e incorpora modalidades sem degradar crenças consolidadas;
- baseline: `fixed_attention_v0`, que mantém pesos anteriores e não ajusta
  confiança;
- métricas: atenção observável, ajuste de confiança, calibração, recuperação,
  bloqueio correto de planos e proveniência;
- ablação: comparar o ganho de atenção observável do tratamento com o baseline
  fixo sob a mesma remoção de modalidade;
- falsificação: remoção de capacidade interrompe o núcleo, gera observação
  inventada, não bloqueia plano incompatível ou exige novo agente.

## Consequências

Positivas:

- perda e retorno de capacidade são explicitamente reversíveis;
- nova modalidade não influencia crenças estáveis antes da calibração;
- limitações e planos incompatíveis ficam auditáveis.

Custos:

- a referência não implementa captura, atuadores ou calibração física;
- confiança ajustada é operacional e precisa de avaliação científica posterior;
- equivalência com plasticidade biológica não é assumida.

## Reversão

Desabilitar o adaptador deixa o runtime de capacidades da SPEC-023 intacto.
Limpar seus perfis remove somente ajustes derivados; histórico de capacidades e
fontes originais permanecem preservados pelo runtime.
