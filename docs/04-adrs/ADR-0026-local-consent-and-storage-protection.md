# ADR-0026 — Consentimento e proteção do armazenamento local

Status: accepted
Data: 2026-07-29
Decisores: aprovação humana do projeto

## Contexto

O Runtime Preview já persiste eventos locais, mas ainda não possui uma
política operacional para decidir quando uma capacidade pode capturar,
interromper a captura, exportar ou excluir dados. Retenção cognitiva não é
equivalente a governança de dados pessoais. A quota também não pode provocar
eliminação silenciosa para manter o processo funcionando.

## Decisão

- O padrão de consentimento é negar.
- Consentimento é registrado por `sensor_id` e `purpose`, com versão e
  histórico de concessões/revogações.
- Uma pausa global bloqueia toda captura nova e pode ser removida sem recriar
  concessões revogadas.
- Dados locais do Windows serão protegidos com DPAPI no escopo do usuário; o
  runtime não fornece fallback criptográfico silencioso quando DPAPI não está
  disponível.
- Defaults de retenção e quota são versionados: bruto 30 dias, derivado 365
  dias, quarentena 14 dias e quota do usuário 10 GiB.
- Banco, WAL, índices, quarentena, backups e payloads contam para a quota. O
  payload de modelo é contabilizado separadamente.
- Ao exceder a quota, a transação em andamento pode concluir atomicamente;
  novas capturas são suspensas, o estado fica `degraded` e uma decisão do
  usuário é exigida. Nenhum dado é apagado automaticamente.
- Exportação e exclusão exigem uma solicitação explícita e confirmada. A
  recuperação usa backup local conhecido, preserva o arquivo afetado em
  quarentena e verifica a cópia antes da troca.

## Consequências

Positivas:

- captura e finalidade ficam auditáveis;
- revogação não depende de apagar imediatamente todo o histórico;
- falta de espaço é observável e reversível;
- o runtime não precisa de rede, serviço externo ou fallback inseguro.

Custos:

- o Windows possui um gate adicional de DPAPI;
- a política precisa ser migrada por versão;
- exportação e exclusão exigem confirmação e podem interromper o fluxo.

## Plano de reversão

Desabilitar a captura e manter somente diagnóstico/replay do Runtime Preview.
Não remover dados persistidos durante a reversão; a exclusão segue apenas uma
solicitação confirmada.
