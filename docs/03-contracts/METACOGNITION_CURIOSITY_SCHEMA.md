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

## Promoção nativa SPEC-039

O candidato C++ `cognition.metacognition_curiosity.v1` expõe a mesma
semântica por `promotion_fixture_runner --metacognition-curiosity`. Ele
preserva provenance entre hipótese, assessment, pergunta e resposta,
versiona a política de calibração/ganho e mantém budget, cooldown e supressão
auditáveis. `inconclusive` não atualiza a calibração como outcome negativo.

O manifesto e os fixtures bloqueados estão em
`promotions/cognition.metacognition_curiosity.v1.json`,
`validation/equivalence/metacognition_curiosity_v1.jsonl` e
`validation/holdout/metacognition_curiosity_v1_holdout.jsonl`. A promoção
comprova equivalência computacional, invariantes e desempenho operacional;
não disponibiliza o componente no produto nem sustenta alegações de emoção,
consciência, intenção ou curiosidade humana.
