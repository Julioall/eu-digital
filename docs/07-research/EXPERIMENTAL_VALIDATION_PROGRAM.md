# Programa de Validação Experimental

## 1. Princípios

- testar função, não aparência;
- comparar com baselines simples;
- registrar resultados negativos;
- usar seeds e sessões reproduzíveis;
- separar desenvolvimento de avaliação;
- avaliar longitudinalmente;
- fazer ablações causais;
- não usar conversa persuasiva como métrica.

## 2. Hipóteses falsificáveis

### H1 — Fusão multimodal

A combinação de eventos do sistema, interação, OCR e áudio melhora a segmentação de episódios em relação a qualquer modalidade isolada.

**Métrica:** boundary F1, WindowDiff e concordância com anotadores.  
**Falsificação:** fusão não supera o melhor sensor isolado.

### H2 — Memória dupla

Memória episódica + semântica com replay preserva conhecimento e generaliza melhor que um armazenamento único.

**Métrica:** retenção, backward transfer, Recall@k, acurácia após mudança de domínio.  
**Falsificação:** não reduz esquecimento ou causa interferência maior.

### H3 — Workspace limitado

Um workspace de capacidade limitada melhora eficiência e robustez em relação a contexto ilimitado ou arquitetura sem broadcast.

**Métrica:** desempenho por RAM/latência, resistência a ruído, tarefa primária.  
**Falsificação:** não há ganho ou há degradação consistente.

### H4 — Metacognição

Um módulo de segunda ordem melhora calibração, reduz sugestões injustificadas e detecta erros sem reduzir excessivamente a cobertura.

**Métrica:** Brier, ECE, AUROC, risk–coverage.  
**Falsificação:** confiança continua desacoplada da correção.

### H5 — Curiosidade por ganho

Perguntas selecionadas por ganho informacional produzem mais aprendizagem por interrupção que perguntas aleatórias ou baseadas apenas em frequência.

**Métrica:** ganho posterior, utilidade humana, redundância e interrupções.  
**Falsificação:** não supera os baselines.

### H6 — Self-model causal

O self-model melhora atribuição de capacidade, explicação de limites, seleção de ações e recuperação autobiográfica.

**Métrica:** consistência, acurácia de capacidade, agência e desempenho.  
**Falsificação:** sua remoção não altera nenhuma medida relevante.

### H7 — Agência digital

Ações reversíveis com previsão de efeito melhoram distinção entre mudanças causadas pelo agente e mudanças externas.

**Métrica:** F1 de atribuição causal, erro preditivo e adaptação.  
**Falsificação:** desempenho igual ao observador passivo.

### H8 — Consolidação

Replay e consolidação durante ociosidade reduzem esquecimento e contradições.

**Métrica:** retenção longitudinal e taxa de contradição.  
**Falsificação:** não há benefício ou memórias falsas aumentam.

### H9 — World model

A previsão explícita de próximos estados melhora detecção de novidade e qualidade de perguntas.

**Métrica:** log loss, top-k, detecção de mudança e ganho de pergunta.  
**Falsificação:** clusters sem previsão têm resultado equivalente.

### H10 — LLM como linguagem, não núcleo

Remover o LLM degrada nomes e diálogo, mas não destrói métricas fundamentais de eventos, memória e padrões.

**Falsificação:** todo funcionamento desaparece sem o LLM.

### H11 — Continuidade autobiográfica

Indexação de eventos como pertencentes ao agente melhora recuperação temporal e coerência narrativa sem aumentar confabulação.

**Métrica:** temporal QA, proveniência e contradições.  
**Falsificação:** narrativa aumenta erros sem ganho funcional.

### H12 — Viabilidade local

O sistema mantém coleta e cognição básica na infraestrutura alvo.

**Metas iniciais:** RAM média abaixo de 12 GB, ausência de swap sustentado, ingestão sem perda crítica, modelo pesado sob demanda.

## 3. Conjuntos de teste

### 3.1 Sandbox sintético

Rotinas programadas com verdade conhecida:

- abertura de aplicativos;
- edição de arquivos;
- alternância de janelas;
- falas simuladas;
- interrupções;
- padrões recorrentes;
- mudanças de rotina;
- eventos causados pelo agente.

### 3.2 Sessões humanas anotadas

Sessões curtas, anotadas por pelo menos duas pessoas:

- limites de episódio;
- objetivo aparente;
- contexto;
- relevância;
- repetição;
- resultado.

### 3.3 Sessões longitudinais

Coleta em 7, 30 e 90 dias para avaliar:

- deriva;
- consolidação;
- esquecimento;
- estabilidade de padrões;
- adaptação;
- utilidade de perguntas.

### 3.4 Benchmarks externos

Na fase de ação:

- OSWorld;
- WindowsAgentArena;
- tarefas locais próprias com avaliadores executáveis.

## 4. Métricas

### Percepção

- precisão de OCR;
- cobertura de eventos;
- alinhamento temporal;
- taxa de duplicação;
- latência.

### Episódios

- boundary precision/recall/F1;
- WindowDiff;
- coerência humana;
- estabilidade sob replay.

### Memória

- Recall@k;
- MRR;
- precisão de proveniência;
- retenção;
- taxa de contradição;
- custo de armazenamento.

### Padrões

- precision/recall de recorrências;
- estabilidade;
- tempo para descoberta;
- false discovery rate;
- adaptação a drift.

### Previsão

- top-k accuracy;
- log loss;
- Brier score;
- surpresa calibrada;
- detecção de mudança.

### Metacognição

- ECE;
- Brier;
- AUROC de erro;
- meta-d' quando aplicável;
- risk–coverage;
- taxa correta de abstinência.

### Curiosidade

- ganho por pergunta;
- taxa de resposta;
- redundância;
- interrupções por hora;
- valor humano percebido;
- alteração posterior do modelo.

### Self-model

- acurácia das capacidades;
- consistência temporal;
- atribuição de agência;
- recuperação de versão;
- impacto causal por ablação.

## 5. Matriz mínima de ablação

| Experimento | Completo | Sem memória episódica | Sem semântica | Sem workspace | Sem self-model | Sem metacognição | Sem LLM |
|---|---:|---:|---:|---:|---:|---:|---:|
| Segmentação | ✓ |  |  |  |  |  | ✓ |
| Recuperação | ✓ | ✓ | ✓ |  |  |  | ✓ |
| Previsão | ✓ | ✓ | ✓ | ✓ |  |  | ✓ |
| Perguntas | ✓ |  |  | ✓ | ✓ | ✓ | ✓ |
| Agência | ✓ |  |  |  | ✓ | ✓ | ✓ |
| Narrativa | ✓ | ✓ | ✓ |  | ✓ | ✓ | ✓ |

## 6. Gates científicos

### Gate A — Observação válida

Eventos e episódios atingem baseline definido e replay determinístico.

### Gate B — Memória válida

Recuperação, proveniência e retenção superam baseline de log cronológico.

### Gate C — Aprendizagem válida

Padrões descobertos generalizam e não são apenas repetição literal.

### Gate D — Metacognição válida

Confiança é calibrada e melhora a política de perguntar.

### Gate E — Self funcional mínimo

Self-model e agência possuem impacto causal por ablação.

### Gate F — Sugestão

Sugestões são úteis e não aumentam significativamente interrupção ou erro.

### Gate G — Ação

Agente supera baseline em ambiente executável e mantém segurança.

Nenhum gate autoriza alegação de consciência.

### H13 — Plasticidade de capacidades

O agente mantém integridade e adapta atenção, confiança e planejamento quando sensores são removidos ou adicionados.

**Métrica:** continuidade, erro, calibração, tempo de recuperação e planos bloqueados corretamente.  
**Falsificação:** a remoção de um sensor opcional interrompe o núcleo, gera observações inventadas ou exige recriação de identidade.

### H14 — Onboarding de nova modalidade

Uma modalidade inédita pode ser incorporada progressivamente sem reduzir desempenho consolidado.

**Métrica:** tempo de calibração, ganho incremental, interferência e proveniência.  
**Falsificação:** a modalidade não pode ser usada sem alterações no núcleo ou contamina crenças anteriores.

### H15 — Invariância de implantação

A implementação C++ preserva propriedades validadas no Laboratório.

**Métrica:** equivalência exata, numérica, estatística ou comportamental, mais comparação contra ground truth.  
**Falsificação:** Python e C++ concordam, mas violam a verdade conhecida ou invariantes.

### H16 — Transferência ecológica

Ganhos de sandbox e replay persistem em sensores reais, concorrência, ruído e drift.

**Métrica:** efeito relativo, calibração, falhas, latência e retenção online.  
**Falsificação:** benefício desaparece fora do replay.

### H17 — Continuidade sob mudança de corpo digital

Adicionar ou remover capacidades atualiza o self-model operacional sem fabricar experiência e sem recriar identidade.

**Métrica:** acurácia de capacidade, confiança, bloqueio de planos e continuidade histórica.  
**Falsificação:** observações inventadas, confiança indevida ou ruptura de identidade.

### Gate H — Implantação válida

O componente passa em contrato, ground truth, equivalência cruzada e auditoria de exportação.

### Gate I — Transferência ecológica

O componente mantém efeito em sessão online e avaliação longitudinal.
