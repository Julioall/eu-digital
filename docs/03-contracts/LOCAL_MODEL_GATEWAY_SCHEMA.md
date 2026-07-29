# Contratos: Gateway de Modelo Local

Os schemas executáveis da SPEC-013 são locais e não identificam um runtime ou
modelo concreto:

- `model_prompt_template.schema.json`: template imutável por identificador e
  versão;
- `local_model_request.schema.json`: requisição enfileirada, backend/modelo
  selecionados localmente, prioridade, timeout e prompt renderizado;
- `local_model_response.schema.json`: resposta concluída com formato
  `output.kind` e `output.fields` validado antes de devolução.

O conteúdo do prompt permanece no processo local. O contrato não autoriza
envio de dados à rede, ações, diálogo autônomo ou uso de uma API. Saída que não
respeita o formato estruturado é rejeitada e não convertida em texto livre.
