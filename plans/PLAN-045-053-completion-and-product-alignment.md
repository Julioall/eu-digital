# Plano de conclusão das SPECs 045–053 e alinhamento com a visão do produto

Status: proposed
Escopo: planejamento documental; nenhuma implementação ou promoção de status
SPECs cobertas: SPEC-045 a SPEC-053

## Resultado pretendido

Concluir o ciclo atual de integração sem transformar o projeto em um chatbot ou
em automação por regras. Ao fim desta sequência, o produto deve conseguir, com
consentimento e integralmente local:

1. observar eventos de baixo risco do usuário;
2. segmentar esses eventos em episódios;
3. recuperar memória e atualizar padrões incrementais;
4. prever transições e medir erro/incerteza;
5. decidir entre permanecer em silêncio, perguntar ou sugerir;
6. explicar a evidência e as limitações da decisão;
7. preservar estado e reconstruí-lo após reinício;
8. receber correção do usuário e incorporá-la em ciclos posteriores;
9. preparar ações por capacidades removíveis e executá-las somente após
   simulação, política e confirmação explícita.

Este lote não autoriza autonomia irrestrita, execução destrutiva, telemetria,
envio de dados à nuvem ou substituição dos módulos cognitivos por um LLM.

## Diagnóstico do estado atual

| SPEC | Estado documental | Evidência existente | Lacuna que impede conclusão |
| --- | --- | --- | --- |
| 045 | `done` | contratos 1.0, coordenador, integração no host, benchmark, validação operacional e relatório | nenhuma pendência crítica na SPEC; replay persistente permanece exclusivamente na SPEC-046 |
| 046 | `done` | checkpoint 2.0, replay sem efeitos, crash recovery, DPAPI, testes e relatório | nenhuma pendência crítica na SPEC |
| 047 | `done` | portas, adaptadores, pattern port, factory, hot-plug, benchmark, testes e relatório | nenhuma pendência crítica na SPEC |
| 048 | `done` | ADR-0035, schemas 1.0, parser estrito, isolamento assíncrono, Qt por porta, testes e relatório | nenhuma pendência crítica na SPEC; backend/modelo concreto permanece na SPEC-051 |
| 049 | `draft` | dispatcher, feedback e relatório existem | adiciona `outcome_unknown` e taxas de confiança sem versionamento contratual, hipótese, baseline, métrica, ablação ou falsificação |
| 050 | `draft` | entrypoint, controller, teste e relatório existem | não há evidência dos limites p99, idle, ausência de deadlock, consentimento por sensor e recuperação de crash exigidos pela SPEC |
| 051 | `done` | backend Ollama e teste existem | dois critérios permanecem desmarcados; não existe ADR autorizando o backend/API concretos, em conflito com ADR-0015 |
| 052 | `cancelled` | UI e relatório de implementação existem | o corpo ainda diz `implemented`; deve ser preservada como spike/superseded, não promovida como produto |
| 053 | `draft` | UI de atividade/cards e integração parcial existem | critérios são duplicados e contraditórios; faltam schemas versionados, proveniência, métricas e prova de aprendizagem longitudinal real |

Os relatórios existentes são evidência de trabalho realizado, mas não substituem
critérios de aceite, execução reproduzível dos testes ou o registro de
maturidade da ADR-0025.

## Bloqueios de governança a resolver primeiro

Nenhuma SPEC deste lote deve ser promovida enquanto os itens abaixo não forem
decididos e registrados:

1. Aprovar ou rejeitar uma ADR específica para Ollama, incluindo loopback
   apenas, seleção por configuração, ausência de download implícito, modelo
   permitido, licença, timeout, cancelamento, unload e reversão. Até lá, o
   backend permanece experimental e desabilitado por padrão.
2. Resolvido pela ADR-0035: `contracts/schemas` é a fonte normativa;
   `docs/03-contracts` documenta e `schemas` permanece legado não normativo.
3. Versionar a extensão de contratos de ação antes de introduzir
   `outcome_unknown` e documentar compatibilidade com registros antigos.
4. Registrar protocolo científico para a política de atualização de confiança
   da SPEC-049. Constantes empíricas não podem entrar no runtime como evidência
   cognitiva sem baseline e critério de falsificação.
5. Resolvido pelas SPECs 045/047: aprendizagem incremental é uma capacidade
   removível própria e participa do ciclo por DTOs versionados.
6. Reescrever os critérios da SPEC-053 para que sejam únicos, mensuráveis e
   rastreáveis a contratos e testes.

## Fluxo-alvo mínimo

```text
sensores consentidos
  -> CanonicalEvent + ObservationEnvelope
  -> timeline local
  -> segmentação de episódios
  -> memória episódica e recuperação
  -> aprendizagem incremental de padrões
  -> world model + erro preditivo
  -> workspace limitado
  -> metacognição/curiosidade
  -> self-model funcional
  -> decisão: silêncio | pergunta | sugestão | preparar ação
  -> apresentação local ou gate de ação supervisionada
  -> feedback/outcome como novo CanonicalEvent
```

Cada seta deve preservar proveniência, versão, confiança e ausência explícita.
O renderer linguístico apenas comunica uma decisão estruturada; ele não cria
memórias, crenças, planos ou fatos por conta própria.

## Ordem de conclusão

### Marco 0 — Baseline e saneamento documental

Objetivo: tornar o estado real auditável antes de alterar comportamento.

Ações:

- executar lint, tipos, testes Python, build C++, CTest, validação de schemas e
  gates de promoção sem alterar holdouts;
- produzir uma matriz “critério → teste → evidência → status” para cada SPEC;
- corrigir metadados contraditórios, dependências e referências de contratos;
- manter como pendente todo critério sustentado apenas por um relatório antigo;
- atualizar o registro de maturidade sem promover componentes por declaração;
- manter SPEC-052 cancelada e registrar sua relação de supersessão com a 053.

Saída: baseline reproduzível e lista fechada de lacunas por SPEC.

Critério de parada: qualquer contrato público sem origem canônica, mudança
arquitetural sem ADR ou teste que dependa de rede/serviço não controlado.

### Marco 1 — Fechar SPEC-047: portas, contratos e substituição

Objetivo: garantir que o núcleo conheça funções, nunca implementações concretas.

Ações:

- reconciliar as oito portas existentes com os contratos executáveis;
- adicionar a função de aprendizagem de padrões somente após a decisão do
  bloqueio 5, com DTO versionado e adapter para o componente promovido;
- registrar adaptadores por `CapabilityDescriptor` e operação;
- provar ausência, falha, remoção, reinstalação e substituição em runtime;
- provar atualização do self-model e invalidação de planos ao mudar uma
  capacidade;
- manter componentes concretos fora dos headers do coordenador.

Testes obrigatórios:

- unitários por adapter e mapeamento sem perda dos DTOs;
- contrato e compatibilidade de versões;
- integração com registry e hot-plug/restart-safe;
- metamórficos de ordem irrelevante de registro;
- benchmark da chamada por porta contra a baseline concreta.

Saída: SPEC-047 elegível para `done`, sem promover automaticamente componentes
para produto.

### Marco 2 — Fechar SPEC-045: ciclo cognitivo observável

Objetivo: processar eventos de ponta a ponta com degradação previsível.

Ações:

- ligar as portas na ordem do fluxo-alvo aprovado;
- definir etapas críticas e opcionais sem fallback silencioso;
- implementar timeout cooperativo, fila limitada, idempotência e marcação de
  eventos internos;
- separar estado operacional do ciclo de qualquer alegação cognitiva;
- congelar baseline sem cada módulo e protocolo de ablação;
- emitir `CognitiveCycleResult` estruturado com evidências, ausências e erros.

Testes obrigatórios:

- sequência completa e chamadas exatamente uma vez;
- fila cheia, duplicata, timeout e cancelamento cooperativo;
- falha/ausência de cada porta isoladamente;
- reentrada de eventos internos sem loop infinito;
- determinismo sob replay da mesma sequência;
- integração com pelo menos duas implementações reais e demais fixtures.

Saída: um evento observado produz memória/predição/decisão auditáveis, sem UI e
sem depender do backend linguístico.

### Marco 3 — Fechar SPEC-046: continuidade e aprendizagem entre sessões

Objetivo: o agente não perder o estado funcional ao reiniciar.

Ações:

- definir quais estados são reconstruíveis pela timeline e quais exigem
  snapshot versionado;
- reidratar os componentes exclusivamente por suas portas;
- reaplicar eventos posteriores ao cursor pelo coordenador em modo
  `fast-forward`, sem sugestões, apresentação ou ações;
- provar escrita atômica, proteção local, checksum, snapshot anterior e cold
  start limitado;
- impedir que replay duplique memória, feedback, perguntas ou ações;
- separar snapshot operacional da avaliação longitudinal de 7/30/90 dias.

Testes obrigatórios:

- abort entre eventos e restauração exata;
- corrupção, versão incompatível, configuração diferente e snapshot expirado;
- replay repetido idempotente;
- equivalência entre execução contínua e snapshot + replay;
- teste metamórfico de particionamento da mesma timeline;
- medição de pausa e uso de memória durante cópia assíncrona.

Saída: episódios, padrões, predições e self-model continuam coerentes entre
sessões; a timeline permanece a fonte da verdade.

### Marco 4 — Fechar SPEC-048 e regularizar SPEC-051: linguagem como renderer

Estado: SPEC-048 concluída; regularização da SPEC-051 permanece pendente e não
foi antecipada.

Objetivo: comunicar decisões sem entregar o núcleo cognitivo ao modelo local.

Ações:

- validar a resposta real contra o schema executável, não apenas verificar que
  o arquivo de schema existe;
- isolar transporte, serialização, timeout e cancelamento no backend;
- tornar o backend removível e selecionável por configuração;
- manter silêncio/fallback seguro quando o modelo estiver ausente ou inválido;
- distinguir resposta solicitada, pergunta cognitiva e sugestão proativa;
- concluir a SPEC-051 somente após ADR e todos os critérios comprovados.

Testes obrigatórios:

- transporte mockado sem Ollama instalado e sem acesso externo;
- JSON válido, truncado, malformado, campos extras, timeout e cancelamento;
- remoção/substituição do backend em repouso;
- prova de que timeout não bloqueia a thread coordenadora;
- orçamento proativo invariável para respostas solicitadas;
- ausência de memória ou fato novo originado apenas no texto do modelo.

Saída: diálogo local opcional, estruturado e degradável.

### Marco 5 — Fechar SPEC-050 e consolidar SPEC-053: companheiro observador

Objetivo: oferecer uma experiência útil de observação e assistência contextual,
sem mascarar dados simulados como aprendizado.

Ações:

- tratar SPEC-052 como spike cancelado e reutilizar apenas artefatos compatíveis;
- criar schemas versionados para `CurrentActivity` e
  `ContextualAssistanceCard` na pasta canônica;
- derivar `CurrentActivity` de episódios/eventos com proveniência e confiança,
  nunca de texto livre do LLM ou valor fixo de UI;
- gerar cards apenas de `CognitiveDecision` validada, expondo motivo,
  confiança, fontes e opção de corrigir/silenciar;
- converter correções, deferimentos e silêncio em eventos de feedback para o
  ciclo cognitivo;
- implementar consentimento por sensor/finalidade, pausa global, revogação e
  visibilidade de capacidades conforme ADR-0026/0027;
- provar separação entre thread Qt e thread cognitiva;
- manter a aplicação funcional sem modelo, OCR, áudio ou qualquer plugin
  opcional.

Testes obrigatórios:

- primeira execução deny-by-default e ausência total de captura antes do aceite;
- concessão, pausa, revogação, remoção, reinstalação e substituição por sensor;
- evento observado → atividade com proveniência → decisão → card;
- correção do card → novo evento → atualização posterior observável;
- ausência de chamada direta UI → Ollama;
- responsividade do tray p99, frame time, idle CPU/RAM, shutdown e recovery;
- acessibilidade, IME pt-BR, DPI, multi-monitor e lifecycle conforme ADR-0032.

Saída: a SPEC-050 fecha o produto mínimo; a SPEC-053 fecha a experiência de
companheiro. Ambas permanecem separadas dos estados de promoção científica.

### Marco 6 — Fechar SPEC-049: preparar o caminho para ferramentas futuras

Objetivo: integrar feedback de ações sem habilitar automação genérica.

Ações:

- versionar o contrato de outcome e reconciliar registros pendentes após crash;
- substituir constantes arbitrárias de confiança por política configurável e
  protocolo científico aprovado;
- vincular idempotência ao plano canônico completo e à autorização consumida;
- impedir reentrada recursiva de outcomes;
- manter execução real desabilitada na ausência de um plugin de ferramenta;
- exigir, para cada ferramenta futura, `CapabilityDescriptor`, política,
  simulação, confirmação, auditoria e rollback declarado.

Testes obrigatórios:

- prepare não executa; autorização inválida/expirada/digest diferente bloqueia;
- repetição é `at most once` inclusive após reinício;
- crash gera `outcome_unknown` e reconciliação manual;
- ausência/falha/remoção/reinstalação/substituição do atuador;
- ablação da política de feedback e curva de calibração contra baseline;
- nenhuma ação destrutiva ou concreta em testes do núcleo.

Saída: infraestrutura pronta para SPECs futuras de ferramentas atômicas; cada
função será um plugin opcional, não uma lista fixa de tarefas no núcleo.

## Validação longitudinal do aprendizado

A conclusão técnica das SPECs 045–053 não prova que o agente aprende melhor ao
longo do tempo. Antes de liberar sugestões como produto, executar o protocolo
congelado da SPEC-022 em sessões locais:

| Horizonte | Evidência mínima |
| --- | --- |
| sessão | ingestão, segmentação, recuperação, atualização de padrão e predição reproduzíveis |
| 7 dias | retenção, precisão de recuperação, log loss, Brier/ECE, perguntas úteis e correções incorporadas |
| 30 dias | estabilidade/drift de padrões, contradições, esquecimento e redução de perguntas redundantes |
| 90 dias | retenção longitudinal, backward transfer, calibração e comparação com baseline/ablação congelados |

Métricas de CPU, RAM, latência e estabilidade são gates operacionais e não
podem ser apresentadas como evidência cognitiva. Falta de observação permanece
ausência, nunca resultado negativo.

## Gates de produto

1. **Observe:** consentimento, timeline e episódios confiáveis.
2. **Learn:** padrões e predições melhoram contra baseline em holdout fechado.
3. **Ask:** perguntas têm ganho informacional e feedback auditável.
4. **Suggest:** sugestões são calibradas, explicáveis, corrigíveis e limitadas.
5. **Prepare:** planos são simulados e apresentados sem executar.
6. **Execute supervised:** um plugin específico executa somente após confirmação.
7. **Limited autonomy:** somente em futura ADR/SPEC, com orçamento, política,
   reversibilidade e aprovação humana explícita.

Não avançar de gate por qualidade visual, fluência do modelo ou simples passagem
de testes de equivalência Python/C++.

## Definition of Done do lote

O lote 045–053 estará concluído somente quando:

- cada SPEC não cancelada possuir status coerente, critérios únicos e relatório
  baseado no template;
- todas as dependências e contratos estiverem versionados e localizáveis;
- ADRs bloqueadoras estiverem aprovadas por humano;
- lint, tipos, unitários, integração, contratos, metamórficos e testes de
  recuperação passarem;
- o fluxo observado puder ser reconstruído localmente com proveniência;
- a aprendizagem incremental estiver conectada e mensurada contra baseline;
- ausência de plugins mantiver o núcleo íntegro e funcional;
- nenhuma ação real ocorrer sem capacidade, política, simulação e confirmação;
- documentação, roadmap, registro de maturidade e relatórios refletirem o mesmo
  estado;
- nenhuma pendência crítica permanecer aberta.

## Próxima decisão humana

Antes da primeira implementação deste plano, aprovar:

1. a estratégia para incluir aprendizagem de padrões no coordenador;
2. a ADR do backend Ollama ou sua remoção do caminho de produto;
3. a pasta canônica de contratos;
4. a revisão/versionamento do contrato de outcomes da SPEC-049.
