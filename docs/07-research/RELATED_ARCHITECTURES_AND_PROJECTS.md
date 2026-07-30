# Arquiteturas e Projetos Relacionados

## 1. Comparação resumida

| Sistema | Principal contribuição | Aproximação com o projeto | Limitação relevante |
|---|---|---|---|
| LIDA | ciclo cognitivo, atenção, GWT, múltiplas memórias, ação | muito alta no desenho cognitivo | implementação completa e aprendizagem real continuam difíceis |
| Soar | arquitetura madura, memória procedural, semântica e episódica | alta em memória e decisão | geralmente exige conhecimento e objetivos mais definidos |
| ACT-R | modelo computacional validado de cognição humana | média como referência científica | não foi criado como agente aberto continuamente desenvolvimental |
| OpenCog/CogPrime | integração de processos, atenção, linguagem, self/outro | alta em ambição | grande complexidade e implementação parcial |
| Dossa et al. | agente multimodal com workspace testado em 3D | alta para workspace e multimodalidade | domínio limitado e sem self autobiográfico |
| Huang et al. | workspace + memória episódica em robô | alta para integração memória/workspace | escala e generalidade limitadas |
| Always-On | percepção contínua, fusão multimodal e consolidação | muito alta no princípio “observar antes de agir” | resultado inicial simples; sistema emergente recente |
| Generative Agents | memória, reflexão e planejamento longitudinal | alta para narrativa e recuperação | mundo simulado e pouco grounding sensório-motor |
| MemGPT | memória hierárquica e controle de contexto | alta para gerenciamento de memória | não é arquitetura cognitiva desenvolvimental completa |
| Voyager | currículo automático, skill library e lifelong learning | alta para aprendizagem procedural futura | ambiente de jogo e dependência de LLM externo |
| Agent S/S2 | uso de computador, experiência e planejamento hierárquico | alta para fase autônoma | orientado por tarefa, não por desenvolvimento aberto |
| OSWorld | benchmark de tarefas reais de computador | essencial para avaliação futura | mede ação, não memória autobiográfica ou self |
| AURA/Tripix | projetos exploratórios com self, memória e estados internos | proximidade conceitual | evidência científica e revisão limitadas |

## 2. LIDA

LIDA é o precedente teórico mais próximo. A arquitetura combina percepção, atenção, broadcast global, memória episódica, memória procedural, seleção de ação, motivação e aprendizagem [R10, R18].

### O que adotar

- ciclo cognitivo recorrente;
- competição por atenção;
- broadcast limitado;
- múltiplas memórias;
- ação como parte do ciclo;
- aprendizagem distribuída.

### O que não copiar diretamente

- terminologia de consciência como conclusão;
- dependência excessiva de estruturas conceituais ainda não implementadas;
- pressuposto de que todo aprendizado relevante exige broadcast.

## 3. Soar

Soar possui décadas de desenvolvimento e memória episódica que registra o fluxo de experiência, além de memória semântica e procedural [R15].

### O que adotar

- separação entre tipos de memória;
- registro automático de episódios;
- recuperação por pistas;
- ciclo decisório explícito;
- mecanismos auditáveis.

### Diferença

O projeto busca descobrir atividades sem conhecimento procedural inicial detalhado. Soar normalmente opera melhor quando regras e objetivos já foram definidos.

## 4. ACT-R

ACT-R é uma teoria e plataforma para modelar cognição humana, com forte tradição experimental [R16].

### Uso recomendado

- referência para limites de memória, latência e mecanismos cognitivos;
- inspiração para experimentos controlados;
- não usar como implementação central do agente.

## 5. OpenCog/CogPrime

OpenCog explora sinergia de múltiplos processos, atenção, linguagem, social modeling e ação [R17].

### Lição

Arquiteturas muito abrangentes podem falhar por integração excessiva antes de validar mecanismos mínimos. O projeto deve preservar incrementalidade e interfaces substituíveis.

## 6. Global Workspace Agent de Dossa et al.

O trabalho implementou um workspace em agente audiovisual 3D e encontrou desempenho e robustez superiores em condições de memória limitada, comparado a uma arquitetura recorrente [R02].

### Lição

- testar workspace por ablação;
- limitar capacidade;
- não assumir que atenção útil surge sem complexidade de tarefa e regularização.

## 7. Robô de Huang, Chella e Cangelosi

Integra workspace e memória episódica, demonstrando armazenamento e recuperação estática, temporal e contextual [R09].

### Lição

Memória deve participar da formação, manutenção e recuperação do conteúdo ativo, e não ser apenas banco consultado pelo LLM.

## 8. Always-On Cognitive Architecture

O projeto do Italian Institute of Technology é o mais próximo da ideia de percepção contínua. Ele combina fusão sensorial, representação multimodal em memória e auto-organização de experiências. Em testes no laboratório e em conferência, aprendeu a distinguir períodos sociais “animados” de períodos “calmos” [R19].

### Lição

A observação contínua pode formar contexto emergente simples. O nosso projeto deve começar com distinções básicas mensuráveis antes de buscar conceitos abstratos.

## 9. Generative Agents

A arquitetura usa observações, memória completa, recuperação dinâmica, reflexão e planejamento. Ablations mostraram que observação, reflexão e planejamento contribuem para comportamento considerado crível [R20].

### Lição

- memória e reflexão podem gerar continuidade;
- “crível” não equivale a cognitivamente correto;
- reflexões do LLM devem manter proveniência e ser testadas.

## 10. MemGPT

MemGPT gerencia contexto como memória virtual, movendo informações entre camadas e usando interrupções [R21].

### Lição

Separar memória ativa e memória de longo prazo é útil. Entretanto, a política de memória não deve depender apenas do LLM.

## 11. Voyager

Voyager combina currículo automático, biblioteca de habilidades e auto-verificação. Demonstrou transferência e aquisição aberta de habilidades no Minecraft [R22].

### Lição

Na fase de autonomia, habilidades devem ser armazenadas como procedimentos reutilizáveis, testados e composicionais.

## 12. Agent S e Agent S2

Esses sistemas usam experiências anteriores, planejamento hierárquico e componentes especializados para controlar interfaces gráficas [R23, R24].

### Lição

A fase de ação deve usar composição de módulos e avaliações executáveis. Um único modelo generalista é insuficiente para grounding e horizontes longos.

## 13. OSWorld

OSWorld criou um ambiente real de avaliação com aplicações e tarefas executáveis. Na publicação original, humanos completaram mais de 72% das tarefas e o melhor modelo avaliado apenas 12,24%, expondo dificuldades de grounding e conhecimento operacional [R25].

O OSWorld 2.0 estende a avaliação para fluxos profissionais longos; resultados recentes continuam mostrando grande distância para desempenho profissional [R26].

### Lição

Não antecipar autonomia. A observação, memória e planejamento precisam de benchmarks antes da execução real.

## 14. Projetos exploratórios

AURA, Tripix e sistemas semelhantes são úteis como catálogos de ideias: memória persistente, self-model, ciclos de consolidação, “propriocepção” computacional e estados regulatórios. Porém, não devem ser usados como evidência central sem avaliação peer-reviewed e ablações.

## 15. Posição comparativa

A proposta não supera os sistemas relacionados em todos os componentes individualmente. Sua ambição distintiva é integrar:

- percepção contínua do desktop e ambiente;
- memória multimodal autobiográfica;
- aprendizagem de padrões sem tarefas pré-definidas;
- self-model e metacognição explícitos;
- diálogo social;
- operação local;
- evolução para ação supervisionada.

Essa integração pode superar projetos existentes em abrangência pessoal e continuidade, mas inicialmente ficará abaixo deles em validação, escala e desempenho especializado.
