# ADR-0027 — Política de observação Windows de baixo risco

Status: accepted
Data: 2026-07-29
Decisores: aprovação humana do projeto

## Contexto

Os adaptadores Windows conseguem observar processos, janela ativa e eventos
de input. Títulos de janela e clipboard podem conter conteúdo pessoal mesmo
quando parecem metadados. A SPEC-031 precisa definir o que é permitido antes
de conectar adaptadores ao runtime.

## Decisão

- processo executável e categoria são a observação padrão;
- título de janela e clipboard ficam desabilitados por padrão;
- habilitação de conteúdo textual exige allowlist explícita por aplicativo;
- denylist obrigatória bloqueia gerenciadores de senha, sessões privadas e
  nomes sensíveis conhecidos;
- o redator `length-only-v1` nunca publica o título bruto: quando habilitado,
  publica somente um marcador de comprimento;
- pausa global e bloqueio por aplicativo suprimem eventos antes do
  `CanonicalEvent` e atualizam health do sensor;
- o renderer e o núcleo não recebem uma segunda cópia do título ou clipboard.

## Consequências

- o evento padrão não contém título de janela nem clipboard;
- adaptação futura de uma allowlist é auditável e reversível;
- testes precisam cobrir ausência, denylist, pausa e habilitação explícita;
- o processo Windows ainda pode ser visível somente quando não estiver
  bloqueado pelo policy.

## Plano de reversão

Desabilitar a capacidade de observação e manter o runtime em `degraded`;
nenhum evento previamente persistido é reescrito pela troca de policy.
