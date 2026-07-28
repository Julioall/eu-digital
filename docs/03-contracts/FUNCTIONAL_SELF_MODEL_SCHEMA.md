# Contratos: Modelo de si funcional

Os schemas executáveis da SPEC-012 são complementares ao contrato público
`self_model.schema.json` da SPEC-023; não o substituem nem o modificam.

- `self_model_internal_event.schema.json`: mudança local de capacidade ou
  registro de afirmação que inicia uma nova versão;
- `functional_self_model_snapshot.schema.json`: snapshot imutável, encadeado
  à versão anterior, com capacidades e afirmações separadas por classificação;
- `self_model_decision.schema.json`: decisão estrutural explicável que consulta
  um snapshot e não executa ação.

Uma afirmação `fact` descreve informação observada/confirmada, `hypothesis`
permanece proposicional e `configuration` descreve escolha de operação. A
ausência de capacidade no snapshot é `unverified`: não é convertida em prova
de indisponibilidade. Estados explicitamente declarados como degradado,
indisponível ou removido explicam a limitação correspondente.
