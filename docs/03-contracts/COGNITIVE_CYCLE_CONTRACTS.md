# Contratos do ciclo cognitivo integrado

## Escopo

Estes contratos 1.0 definem a orquestração headless da SPEC-045. O coordenador
recebe fatos estruturados, resolve capacidades por operação e registra o que
ocorreu em cada etapa. Ele não extrai significado de payload textual, não cria
hipóteses, saliência ou features e não executa ações.

Os schemas em `contracts/schemas/` são normativos. Os tipos C++ em
`cpp/core/contracts/cognitive_cycle_v1.hpp` representam os mesmos conceitos no
runtime instalado, sem dependência de Python.

## Entrada e proveniência temporal

`CognitiveCycleInput` 1.0 preserva IDs, origem, tipo, sessão, relógio,
modalidade, conteúdo estruturado opcional, features e saliência fornecidas por
capacidades anteriores e cadeia causal. `time_basis` distingue o horário da
fonte (`source_occurred`) do horário local usado como fallback
(`received_fallback`). Ausência de aplicação, documento, feature ou sinal não é
convertida em valor vazio com significado negativo.

O adapter do `RuntimeHost` usa o `occurred_at` do evento canônico quando ele
existe. Eventos legados usam o `observed_at` local configurado e ficam marcados
como fallback. O payload JSON é persistido, mas não é interpretado pelo
coordenador.

## Etapas e capacidades

A ordem 1.0 é:

1. boundary de episódio;
2. escrita e recuperação de memória;
3. extração de features;
4. aprendizagem incremental de padrões;
5. predição;
6. avaliação de saliência;
7. seleção do workspace;
8. formação de hipótese;
9. metacognição;
10. consulta ao self-model;
11. decisão baseada em evidência.

Features, saliência e formação de hipótese são capacidades independentes:
`IObservationFeaturePort`, `ISalienceAssessmentPort` e
`IHypothesisFormationPort`. A memória só recebe um episódio materializado e
válido. Uma entrada ausente produz `skipped_absent_input`; uma capacidade
ausente produz `skipped_unavailable`. Nenhum dos dois estados fabrica uma
observação.

Cada chamada recebe `PortInvocationContext` 1.0 com correlation ID, deadline
monotônico, modo de replay e `stop_token` somente em memória. O token não é
serializado. Timeout cooperativo e implementação que ignora cancelamento são
registrados separadamente por `coordinator_timeout` e
`timeout_uncooperative`; nenhuma thread é abortada.

## Resultado e idempotência

`CognitiveCycleResult` 1.0 contém estado terminal, política, etapas ordenadas,
evidências, erros estruturados, degradações e decisão opcional. Não contém card,
texto renderizado, atividade de UI nem ação. Os estados terminais são
`completed`, `degraded`, `failed` e `discarded`.

O ID do ciclo deriva de `event_id` e `bounded_ports_v1`. A fila é limitada e
usa `drop_newest_v1`: excesso produz `discarded_backpressure`. Duplicata retorna
`discarded_duplicate` e registra o descarte, mas não cria um segundo resultado
para o mesmo ciclo. Eventos `cognitive.cycle.result` e cadeia causal originada
em `cognitive.coordinator` não reentram no pipeline.

Replay executa a mesma ordem e os mesmos contratos, mas não publica decisão,
UI ou ação. A reaplicação de snapshots e timeline permanece na SPEC-046.

## Schemas executáveis

- `cognitive_cycle_input.schema.json`;
- `cognitive_cycle_result.schema.json`;
- `cognitive_cycle_stage.schema.json`;
- `port_invocation_context.schema.json`;
- `episode_segmentation_response.schema.json`;
- `observation_features.schema.json`;
- `salience_assessment.schema.json`;
- `hypothesis_formation.schema.json`.

Fixtures compartilhados estão em
`contracts/fixtures/cognitive_cycle_{input,result}.json`.

## Compatibilidade

As operações contextuais são overloads aditivos nas portas existentes. As
assinaturas legadas continuam compiláveis, mas um `CanonicalEvent` C++ reduzido
não é promovido silenciosamente a entrada cognitiva completa. A integração pode
ser revertida com `RuntimeConfig.enable_cognitive_coordinator=false`.
