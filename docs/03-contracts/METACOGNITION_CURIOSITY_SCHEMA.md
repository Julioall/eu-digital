# Contratos: Metacognição e Curiosidade

Os schemas executáveis e versionados da SPEC-011 ficam em
`contracts/schemas/`:

- `hypothesis.schema.json`: hipótese proposta, evidência favorável e contrária,
  alternativas e proveniência;
- `metacognitive_assessment.schema.json`: confiança bruta/calibrada,
  incerteza, decisão de perguntar ou silenciar e razões;
- `curiosity_question.schema.json`: pergunta estruturada, hipótese de origem,
  ganho estimado, orçamento, cooldown e estado de supressão;
- `curiosity_response.schema.json`: resposta/correção e evidência que atualiza
  calibração e supressão.

Esses contratos são locais. `prompt` é texto fornecido por um chamador; não
autoriza gerar diálogo, enviar mensagem, pesquisar a internet ou agir. Uma
resposta `inconclusive` preserva ausência de evidência e não conta como prova
favorável ou contrária.

Confiança calibrada é uma estimativa operacional contra outcomes verificados,
não certeza, intenção, crença humana ou validação científica suficiente.
