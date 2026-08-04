# Desktop Runtime Contract

Os schemas executáveis da SPEC-050 são:

- `desktop_session_state.schema.json`: estado operacional do host,
  consentimento agregado, sensores ativos, pausa, recovery e disponibilidade
  opcional do modelo;
- `desktop_performance_sample.schema.json`: medição local reproduzível de tray,
  frame, idle e shutdown.

Os pares de consentimento 1.0 do host são
`system_activity/local_activity_observation` e
`input_interaction/local_interaction_observation`. Eles são decisões
independentes; o onboarding pode conceder ambos em uma única ação, mas o
ledger grava dois registros.

O estado desktop não contém texto observado, prompt, título de janela,
clipboard, memória ou conteúdo do modelo. `consent_ready` resume apenas se os
pares requeridos para a sessão estão concedidos; a autoridade normativa
continua sendo `consent_policy.schema.json` e o `ConsentLedger` criptografado.

O shell pode existir em `onboarding` ou `degraded` sem sensores e sem modelo.
Ausência não é observação negativa. Qt recebe somente DTOs validados por portas
de apresentação e nunca chama um backend de modelo diretamente.

Métricas de tray, frame e shutdown usam milissegundos. `idle_cpu` usa percentual
da capacidade lógica do processo no intervalo. Resultados offscreen são gates
reproduzíveis de regressão e não substituem a matriz física de monitores, IME e
tecnologias assistivas.
