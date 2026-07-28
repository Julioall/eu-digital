# SPEC-016 — Ações supervisionadas

Status: future
Fase: 8
Dependências: SPEC-012, SPEC-014
ADRs: novo ADR obrigatório

## Objetivo
Preparar, simular e executar ações após confirmação explícita.

## Requisitos
- Plano estruturado.
- Simulação.
- Política.
- Confirmação.
- Auditoria.
- Rollback quando possível.

## Escopo negativo
Autonomia irrestrita e ações destrutivas sem confirmação.

## Critérios de aceite
- [ ] Nenhuma ação ocorre sem autorização válida.
- [ ] Plano e efeitos são mostrados.
- [ ] Resultado é auditado.
