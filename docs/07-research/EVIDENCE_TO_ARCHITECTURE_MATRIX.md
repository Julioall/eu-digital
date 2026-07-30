# Matriz Evidência → Arquitetura

## Escala

- **A:** evidência empírica peer-reviewed, replicações ou tradição madura.
- **B:** revisão peer-reviewed ou implementação parcial convincente.
- **C:** preprint, benchmark técnico ou extrapolação bem fundamentada.
- **D:** hipótese especulativa sem validação adequada.

| Decisão | Base | Nível | Confiança | Consequência de projeto | Teste obrigatório |
|---|---|---:|---:|---|---|
| Cognição modular | LIDA, Soar, ACT-R, OpenCog | A/B | alta | módulos e contratos separados | ablação por módulo |
| Event sourcing e replay | engenharia + memória episódica | A | alta | eventos imutáveis | replay determinístico |
| Workspace limitado | GWT, Dossa et al. | B | média | capacidade e competição | variar capacidade e remover workspace |
| Metacognição separada | Shea & Frith; Fleming | A/B | alta | confiança como segunda ordem | Brier, ECE, erro detectado |
| Memória episódica separada | Soar, Huang et al., Parisi et al. | A/B | alta | episódios específicos | Recall@k e ablação |
| Memória semântica consolidada | dual-memory e Soar | A/B | alta | conceitos fora do episódio | retenção e generalização |
| Memória autobiográfica | Prescott & Dominey | B | média | indexação “aconteceu comigo” | continuidade e atribuição |
| Replay e consolidação | continual learning | A | alta | ciclo offline | teste de esquecimento |
| Esquecimento controlado | continual learning | A/B | alta | retenção por valor | curva memória–desempenho |
| World model | predictive coding | B | alta | previsão de transições | log loss e surpresa |
| Curiosidade por progresso | Oudeyer e robótica desenvolvimental | B | média | perguntas por ganho | comparação com aleatório |
| Sensor fusion contínua | Always-On e embodied AI | B/C | média | timeline multimodal | ablação de modalidades |
| Self-model operacional | artificial self | B | média | capacidades e limites explícitos | ablação e consistência |
| Agência digital | contingências sensório-motoras | B | alta | ação + efference copy | atribuição ação–efeito |
| LLM como interface | MemGPT, Generative Agents | C | média | síntese e diálogo | baseline sem LLM |
| Skill library | Voyager | C | média | memória procedural futura | transferência de habilidades |
| Planejamento GUI | Agent S/S2 | C | média | ação futura em computador | OSWorld |
| Avatar | HCI, não requisito cognitivo | C/D | baixa | interface desacoplada | utilidade e interrupção |
| “Emoções” regulatórias | LIDA e homeostase artificial | C | baixa/média | chamar de estados regulatórios | impacto causal e estabilidade |
| Consciência fenomenal | nenhuma métrica aceita | D | muito baixa | não alegar | não aplicável |

## Regras derivadas

1. Componentes de nível A/B podem entrar no núcleo, mas ainda precisam de teste no domínio local.
2. Componentes C entram atrás de feature flag e experimento.
3. Componentes D não podem sustentar alegações públicas.
4. Texto gerado pelo LLM não conta como evidência do funcionamento interno.
5. Toda função cognitiva deve possuir baseline, métrica e ablação.
