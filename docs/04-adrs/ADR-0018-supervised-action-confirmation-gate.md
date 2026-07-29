# ADR-0018 — Gate de confirmação para ações supervisionadas

Status: aceito
Data: 2026-07-29

## Contexto

A SPEC-016 precisa preparar e executar ações sem transformar o agente em
automação irrestrita. Um plano deve ser explicado antes da execução, e a
confirmação precisa permanecer vinculada ao plano que foi simulado.

## Decisão

Adotar um fluxo local em quatro fases:

1. prepare: valida o plano, simula efeitos e consulta uma política;
2. authorize: aceita somente autorização explícita, não expirada e vinculada
   ao mesmo plan_id e plan_digest;
3. execute: chama a porta abstrata do atuador somente após autorização válida;
4. rollback: tenta reverter somente ações declaradas reversíveis.

O controlador depende apenas de ActionPort e ActionPolicy. Adaptadores
concretos de teclado, mouse, arquivos ou aplicativos não entram no núcleo e
não são escolhidos nesta SPEC. Toda transição e resultado gera registro
auditável local. Falha ou ausência do atuador bloqueia a execução e não
inventa resultado.

## Regras de segurança

- plano destrutivo não pode ser executado sem política permitida e
  autorização explícita;
- autorização expirada, de outro plano, com digest diferente ou sem
  identificador do autorizador é rejeitada;
- simulação nunca chama execute;
- a autorização é consumida após uma execução ou falha;
- rollback é melhor esforço e seu resultado fica separado do resultado da
  ação;
- nenhum caminho default executa ação real em testes ou sem adaptador local.

## Consequências

Positivas:

- efeitos são apresentados antes de qualquer alteração;
- autorização não pode ser reutilizada em plano modificado;
- ações permanecem substituíveis, auditáveis e testáveis por fixtures;
- ausência de atuador degrada para bloqueio explícito.

Custos:

- cada integração concreta precisa declarar política, simulação e rollback;
- rollback não é garantido por toda operação;
- a confirmação de usuário permanece uma dependência externa ao controlador.

## Reversão

Desabilitar o plugin de ações mantém preparação e simulação locais, mas todas
as execuções retornam bloqueio por ausência de fornecedor. Registros antigos
continuam legíveis pelos contratos versionados.
