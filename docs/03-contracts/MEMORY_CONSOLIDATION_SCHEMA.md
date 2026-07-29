# Contratos: consolidação e esquecimento

Os schemas executáveis são semantic_knowledge.schema.json,
consolidation_record.schema.json e retention_decision.schema.json em
contracts/schemas/.

SemanticKnowledge só é válido com source_episode_ids, support_count e versão.
ConsolidationRecord registra cada replay e seu custo. RetentionDecision torna
explícitos os estados active/archived e exige reversibilidade para archive e
restore.

O contrato não permite apagar a fonte de uma crença consolidada. Hipóteses de
episódios permanecem evidências ou alternativas, não fatos promovidos
automaticamente.
