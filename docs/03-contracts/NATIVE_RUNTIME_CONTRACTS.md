# Contratos do runtime nativo

Os schemas executáveis desta fase são:

- `contracts/schemas/runtime_manifest.schema.json`;
- `contracts/schemas/runtime_health.schema.json`.

## RuntimeManifest

Identifica o binário instalado, o build que o produziu, as versões de
contratos e os componentes promovidos. `python_runtime_dependency` deve ser
sempre `false`.

Um manifesto não autoriza por si só uma promoção. Cada componente listado deve
existir no registro aprovado da SPEC-026.

## RuntimeHealth

É um snapshot operacional local, não uma alegação cognitiva. Ele informa o
ciclo de vida do host, o event bus, a timeline, as capacidades observadas e
erros estruturados.

Uma capacidade ausente deve aparecer como estado explícito ou não aparecer
como capacidade instalada; nunca deve ser convertida em observação negativa.
Um runtime `degraded` continua válido quando somente capacidades opcionais
estão indisponíveis.

Os contratos não incluem dados sensoriais brutos, conteúdo de clipboard,
credenciais ou chamadas de rede.
