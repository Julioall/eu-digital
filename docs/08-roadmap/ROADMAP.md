# Roadmap

## Fase 0 — Fundação documental e técnica
Repositório, validação de specs, contratos, CI, configuração e logging.

## Fase 0.25 — Runtime de capacidades
Registro, discovery, lifecycle, hot-plug, ausência explícita e perfis de degradação.

## Fase 0.5 — Instrumentação científica
Sandbox, corpus anotado, baselines, métricas, ablações e relatórios reproduzíveis.

## Fase 1 — Corpo digital mínimo
Eventos de processo, janela, teclado, mouse, clipboard, arquivos, captura de tela e OCR.

## Fase 2 — Timeline e episódios
Normalização, persistência, correlação, sessões e segmentação.

## Fase 3 — Memória
Memória episódica, semântica, procedural, feedback, consolidação, replay, esquecimento e modelo de si inicial.

## Fase 4 — Cognição mínima
Adaptação a observabilidade parcial, redistribuição de atenção e onboarding de novas modalidades.

Saliência, workspace, padrões, world model, erro preditivo, confiança, metacognição, curiosidade, propriocepção e agência digital.

## Fase 5 — Linguagem e avatar
Modelo multimodal local, diálogo, notificações, explicações e avatar.

## Fase 6 — Aprendizagem longitudinal
Consolidação, esquecimento, contradições, recalibração e avaliação temporal.

## Fase 7 — Sugestão
Geração, ranking e supressão de sugestões.

## Fase 8 — Ação supervisionada
Planejamento, simulação, confirmação, execução e rollback.

## Fase 9 — Sensores ambientais
Áudio contínuo, separação de falantes, câmera, presença e sinais sociais.

## Fase 10 — Autonomia limitada
Políticas, orçamento de ação, permissões, auditoria e desligamento seguro.

## Fase 0.1 — Fundação Laboratório/Cérebro

- monorepositório C++/Python;
- contratos compartilhados;
- build mínimo;
- fixture comum;
- primeiro teste cruzado;
- empacotamento sem Python.

## Fase 0.2 — Pipeline de promoção

- congelamento de referência;
- geração de fixtures;
- equivalência;
- benchmarks;
- registro de componentes aprovados.

## Fase 0.3 — Runtime local operável

- host C++ com ciclo de vida explícito;
- manifesto de build e snapshot de saúde versionados;
- integração controlada com event bus, capabilities e timeline;
- replay determinístico e recuperação após reinício;
- pacote nativo sem Python, rede ou capacidades de domínio obrigatórias.

Escopo detalhado: `specs/SPEC-028-native-runtime-shell.md`.

## Sequência de releases atual

- **Runtime Preview:** SPEC-028 RuntimeHost, SPEC-029 maturidade de
  componentes e SPEC-030 privacidade/armazenamento local.
- **Product Beta — observação:** SPEC-031 observação Windows de baixo risco e
  SPEC-032 OCR consentido independente.
- **Próximo incremento:** SPEC-033, primeira promoção cognitiva atômica;
  nenhuma capacidade cognitiva é disponibilizada no produto apenas por estar
  concluída no laboratório Python.
- **SPEC-033 em validação:** segmentação nativa de episódios, com referência
  congelada, equivalência C++/Python, baseline, ablação e holdout bloqueado.
  O componente permanece `product_status: unavailable` até aprovação humana.
- **SPEC-034 em validação:** memória episódica nativa, com recuperação
  explicável, proveniência, embedding opcional, retenção limitada e holdout
  bloqueado. Consolidação semântica continua fora desta promoção.
- **SPEC-035 em validação:** padrões nativos com suporte, feedback, drift,
  baseline de chave exata e holdout bloqueado. O status `promoted` do cluster
  não significa verdade, nomeação ou ação.
- **SPEC-036 concluída nos gates automatizados:** world model nativo com
  previsão explícita, incerteza, erro, calibração, drift, baseline, ablação e
  holdout. O componente permanece `product_status: unavailable` até revisão
  humana.
- **SPEC-037 concluída nos gates automatizados:** workspace global nativo com
  capacidade limitada, prioridade observada, expiração, proveniência, baseline
  FIFO, ablação, broadcast local e holdout. O componente permanece
  `product_status: unavailable` até revisão humana; SPEC-038 é o próximo
  incremento nativo.
- **SPEC-038 concluída nos gates automatizados:** self-model funcional nativo
  com snapshots imutáveis, histórico causal, decisões condicionadas a
  capabilities, remoção/reinstalação, baseline, ablação e holdout. O componente
  permanece `product_status: unavailable` até revisão humana; SPEC-039 é o
  próximo incremento nativo.
- **SPEC-039 concluída nos gates automatizados:** metacognição e curiosidade
  nativas com assessments calibráveis, ganho informacional, perguntas locais,
  orçamento/cooldown/supressão versionados, feedback inconclusivo preservado,
  baseline, ablação e holdout. O componente permanece
  `product_status: unavailable` até revisão humana.
- **V1 GA:** permanece condicionada a modelo local aprovado, shell/avatar,
  sugestões corrigíveis, MSIX assinado, atualização/rollback e gates de
  privacidade/operacionais.

O status documental das SPECs não substitui `reference_status`,
`native_status` e `product_status` do registro de maturidade.

- **SPEC-040 concluida nos gates automatizados:** gateway local opcional com
  porta C++ removivel, fila de um modelo pesado, timeout/cancelamento/descarga,
  schema estruturado, politica de artefato GGUF e degradacao explicita sem
  modelo. O componente permanece `product_status: unavailable`; modelo,
  runtime de inferencia e assinatura assimetrica de release exigem decisao
  posterior conforme ADR-0015.
