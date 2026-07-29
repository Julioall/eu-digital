# Catálogo de Componentes

| Componente | Responsabilidade | Estado inicial |
|---|---|---|
| Event Bus | Transporte interno | obrigatório |
| Capability Registry | Descoberta e estado de capacidades | obrigatório |
| Plugin Lifecycle Manager | Inicialização, drain e remoção | obrigatório |
| Capability Resolver | Seleção por operação e qualidade | obrigatório |
| Sensor Manager | Ciclo de vida dos sensores | obrigatório |
| Audio Sensor | VAD local, segmentos e transcrição referenciada | fase sensorial |
| Supervised Action Controller | Preparar, confirmar, executar e auditar ações | fase de agência |
| Digital Agency Reference | Propriocepção, cópia eferente e atribuição | laboratório |
| Event Normalizer | Produzir eventos canônicos | obrigatório |
| Timeline Store | Persistir e consultar eventos | obrigatório |
| Episode Segmenter | Gerar episódios | obrigatório |
| Memory Service | Memórias especializadas | obrigatório |
| Salience Engine | Priorizar informações | fase cognitiva |
| Global Workspace | Integrar itens ativos | fase cognitiva |
| Pattern Learner | Clusters e sequências | fase cognitiva |
| Predictor | Próximos eventos | fase cognitiva |
| Curiosity Engine | Perguntas informativas | fase cognitiva |
| Metacognition Engine | Confiança e crítica | fase cognitiva |
| Self Model | Estado e história do agente | fase cognitiva |
| Local Model Gateway | Invocação do modelo multimodal | linguagem |
| Dialogue Manager | Conversa e confirmação | linguagem |
| Avatar UI | Presença visual | linguagem |
| Policy Engine | Limites de ação | autonomia |
| Action Executor | Execução auditável | autonomia |
