---
id: SPEC-046
title: Consistent Cognitive Snapshot and Replay
status: done
phase: design
dependencies: [SPEC-006, SPEC-030, SPEC-045, SPEC-047]
adrs: [ADR-0034]
contracts: [cognitive_snapshot.schema.json, cognitive_snapshot_v2.schema.json, cognitive_state_bundle.schema.json]
---

# SPEC-046 — Consistent Cognitive Snapshot and Replay

Status: done
Owner: humano  
Fase: design  
Dependências: SPEC-006 (Timeline Store), SPEC-030 (Privacy & Storage),
SPEC-045 (Integrated Cognitive Cycle), SPEC-047 (Component Wiring)
ADRs aplicáveis: ADR-0034
Contratos afetados: `cognitive_snapshot.schema.json` legado e novos contratos
2.0 definidos pela ADR-0034.

## Problema
Componentes como o `GlobalWorkspace`, a fronteira ativa do `EpisodeSegmenter` e os temporizadores operam em memória volátil. Reiniciar o sistema ou atualizá-lo resulta em "amnésia" de curto prazo. Uma estratégia simples de salvar o estado a cada N eventos é frágil, pois falhas imprevisíveis fariam perder eventos recentes.

## Objetivo
Criar um mecanismo robusto onde um **Snapshot Consistente** do estado das memórias de curto prazo seja gravado periodicamente. Em caso de *crash* ou reinicialização, o sistema não confia apenas no snapshot: ele carrega o snapshot e **avança o cursor da timeline (replay)** re-aplicando os eventos desde a marca do snapshot até a ponta (último evento gravado). Assim, a fonte da verdade continua sendo o SQLite Timeline.

## Resultado observável
Ao matar violentamente (SIGKILL) o `eu_digital_runtime` e religá-lo, ele restaura a fronteira do episódio com exatidão matemática, processando internamente em *fast-forward* os eventos da timeline que ocorreram após a última gravação do snapshot.

## Requisitos funcionais
- Definir o formato canônico do Snapshot (JSON criptografado, versionado).
- Persistir o Snapshot atomicamente no SQLite (`PrivacyStorage`).
- O Snapshot DEVE conter o `last_applied_event_id`.
- Na inicialização, recuperar o Snapshot, decifrar, aplicar o estado e, em seguida, fazer a leitura iterativa do `TimelineStore` do `last_applied_event_id` até a frente, executando o `CognitiveCoordinator` para recompor a ponta atual.
- Incluir políticas de fallback: se o snapshot atual não puder ser lido, tentar a versão `-1`. Se falhar, descartar e processar a timeline inteira desde a janela limite do Workspace (Cold Start Restrito).

## Requisitos não funcionais
- **Consistência:** A escrita do snapshot no disco não pode interromper ou bloquear o thread do sistema enquanto ele ocorre (deve ser assíncrona com cópia profunda do estado).
- **Versionamento:** O schema deve suportar migrações seguras e *checksum*.
- **Tolerância a falha:** A corrupção de um snapshot nunca pode gerar um crash fatal (fallback para rebuild).

## Entradas
- Eventos de Timer internos mandando gravar o snapshot.
- Inicialização do host ativando a rotina de *Replay*.

## Saídas
- Blob criptografado no banco SQLite.
- Restauração de variáveis internas dos módulos cognitivos via portas.

## Fluxo
1. A cada 100 eventos (ou timeout/desligamento limpo), o Coordenador clona o estado dos módulos em background.
2. Formata para o schema canônico, adiciona hash de integridade e versão.
3. Salva atomicamente na tabela auxiliar.
4. (Crash).
5. Inicialização: Lê a tabela auxiliar, verifica hash, verifica versão.
6. Restaura estado nos componentes.
7. Solicita ao `TimelineStore` os eventos a partir do `last_applied_event_id`.
8. Repassa os eventos ao coordenador no modo *fast-forward*.

## Estados e transições
- `clean`: Estado totalmente salvo e sincronizado.
- `dirty`: Eventos novos ainda não compõem um snapshot.
- `recovering`: O sistema iniciou e está fazendo o replay rápido da timeline.

## Erros esperados
- `SnapshotIntegrityError`: Falha no checksum ou decriptografia (Gera fallback).
- `SnapshotVersionMismatchError`: Falta de backward compatibility (Gera fallback para rebuild).

## Escopo negativo
- Não salvar imagens OCR brutas ou PII de áudio dentro do snapshot (pertencem ao arquivo principal encriptado e não ao buffer cognitivo curto).
- O Snapshot NÃO é a fonte da verdade. É apenas um acelerador de reconstrução.
- Não introduzir formato BSON arbitrário se o projeto usa JSON estrito.

## Critérios de aceite
- [x] O schema `cognitive_snapshot.schema.json` exige versão, timestamp, checksum, fingerprint da configuração e `last_applied_event_id`.
- [x] Escrita atômica garantida: o sistema nunca fica com um snapshot "meio escrito".
- [x] Teste realístico: Processo morre (`abort()`) entre dois snapshots. A reinicialização não perde a segmentação do episódio.
- [x] O sistema aplica fallback graciosamente se injetado lixo binário no registro do snapshot no SQLite.
- [x] Existe controle de expiração (se o último snapshot é velho demais, descartá-lo para evitar reconstrução imensa, dependendo do threshold configurado).

## Plano de testes

### Unitários
- Testar gerador e validador de Checksum e assinaturas de versão.

### Integração
- Forçar kill e checar recomposição.
- Forçar versão de schema incompatível e verificar que o fallback roda.

### Contrato
- O snapshot deve validar rigorosamente sob o seu schema.

### Desempenho
- O overhead de salvar e encriptar não travará (stutter) a coleta principal. Tamanho limite justificado empíricamente.

### Recuperação
- Coberto pelos testes de falha na integração.

## Migração
- Nova tabela `cognitive_snapshots` no SQLite existente (usando script de migração oficial do repositório).

## Rollback
- Drop da tabela auxiliar. O sistema ainda faria cold-start a cada boot (comportamento atual).

## Evidências de conclusão
- Artefato SQLite criado comprovando o tamanho do JSON e sua integridade.
- Logs atestando "Replay of N events completed".
