# Contratos: ações supervisionadas

Os schemas executáveis são action_plan.schema.json,
action_simulation.schema.json, action_authorization.schema.json e
action_outcome.schema.json em contracts/schemas/.

## Fluxo

Um ActionPlan identifica operação, alvo, efeitos solicitados e digest do
plano. ActionSimulation descreve efeitos, risco, reversibilidade e custo
antes da execução. O controlador só aceita ActionAuthorization explícita
para o mesmo plan_id e plan_digest, dentro da validade.

ActionOutcome registra bloqueio, sucesso, falha ou rollback. O resultado da
simulação não é um resultado de execução. A ausência ou falha do atuador é
registrada como bloqueio/falha, nunca como ação realizada.

## Segurança

Os contratos não autorizam ações por si mesmos: a política local, a
confirmação explícita e o controlador devem validar a transição. Nenhum
contrato inclui credenciais ou solicita envio de dados para a nuvem.
