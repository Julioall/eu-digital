# Questões Abertas

1. Sistema operacional inicial: Windows 11 confirmado?
2. GPU dedicada ou integrada disponível?
3. Limite aceitável de armazenamento diário?
4. Frequência máxima de captura visual?
5. Modelo multimodal local definitivo?
6. Banco de dados inicial: resolvido pela SPEC-006 como SQLite puro; Qdrant e
   outros armazenamentos semânticos permanecem fora do escopo inicial.
7. Interface do avatar: Tauri, Qt ou outra?
8. Idioma interno canônico: português ou inglês?
9. Política de retenção de áudio bruto?
10. Métrica humana para utilidade de perguntas?

11. Formato inicial de plugins: entry points Python, manifests em diretório ou subprocessos?
12. Hot-plug será exigido na primeira versão ou apenas restart-safe?
13. Quais operações formam a ontologia inicial de capacidades?
14. Como versionar modalidades novas sem alterar `CanonicalEvent`?
15. Quais sensores serão considerados obrigatórios? Recomendação atual: nenhum sensor de domínio; somente relógio, event bus e estado interno.


## Resoluções registradas pela SPEC-023

11. A primeira implementação suporta os dois mecanismos locais: manifestos
   JSON em diretório e entry points Python. Subprocessos permanecem fora desta
   SPEC.
12. Hot-plug é exigido quando o descritor declara suporte; estados persistidos
   e checkpoints tornam o reinício seguro nos demais casos.
13. Não há ontologia fixa de operações: cada `CapabilityDescriptor` declara as
   operações que fornece, e o resolver seleciona por operação.

## Resoluções registradas pela SPEC-010

Os bloqueios da SPEC-010 foram resolvidos por ADR-0012 e pelos schemas de
workspace versionados:

1. candidatos, itens, snapshots e broadcasts possuem contratos próprios e o
   broadcast usa `CanonicalEvent` sem alterar contratos de evento, episódio ou
   padrão;
2. `observed_weighted_mean_v1` define fatores permitidos, desempate por
   `candidate_id` e ausência explícita fora da média ponderada;
3. o protocolo fixa baseline FIFO, métricas de seleção, ablação configurável,
   holdout anotado e critério de falsificação;
4. a primeira referência é Python, local e efêmera, com snapshots para replay
   determinístico. Promoção C++ permanece sujeita à SPEC-026 e evidência
   independente.

## Resoluções registradas pela SPEC-011

Os bloqueios da SPEC-011 foram resolvidos por ADR-0013 e contratos
versionados:

1. hipótese, avaliação metacognitiva, pergunta estruturada e resposta têm
   schemas executáveis em `contracts/schemas/` e validação local;
2. `bucketed_beta_v1` aprende exclusivamente com outcomes confirmados ou
   rejeitados indexados pela confiança bruta; Brier, ECE, AUROC e
   risk-coverage permanecem métricas registradas para avaliação posterior em
   holdout congelado;
3. `information_gain_v1` produz somente propostas estruturadas e aplica
   orçamento, cooldown, correção, redundância e silêncio. Não há diálogo,
   LLM, busca externa ou ação autônoma;
4. a referência é Python, local e efêmera. Promoção C++ continua condicionada
   à SPEC-026 e a evidência independente; concordância entre runtimes não é
   prova de validade científica.

A questão aberta 10 continua necessária para validar utilidade humana em
estudo posterior, mas não altera a semântica contratual desta SPEC.

## Resoluções registradas pela SPEC-012

Os requisitos arquiteturais da SPEC-012 foram resolvidos por ADR-0014 e
contratos complementares, sem alterar o contrato público de capacidades:

1. `self_model.schema.json` da SPEC-023 permanece compatível; eventos,
   snapshots e decisões funcionais possuem schemas novos e versionados;
2. cada evento interno gera snapshot imutável, recuperável e encadeado por
   hash; persistência longitudinal continua fora desta SPEC;
3. fatos, hipóteses e configuração são coleções distintas; ausência de uma
   capacidade é não verificada, não evidência negativa;
4. a decisão estrutural consulta o snapshot para bloquear disponibilidade
   incompatível e expõe `unconstrained_decision_v0` para ablação. Ela não
   executa ação, importa plugin concreto ou alega subjetividade.

## Resoluções registradas pela SPEC-013

Os requisitos de gateway foram resolvidos por ADR-0015 e contratos locais:

1. o gateway depende de uma porta `LocalModelBackend`, sem escolher, baixar ou
   importar modelo, runtime concreto ou API;
2. um worker único, fila estável, timeout, cancelamento e descarregamento
   mantêm o limite de um modelo pesado por vez;
3. templates possuem identificador/versão e respostas só retornam após validar
   `output.kind` e `output.fields`;
4. backend e política de fila são selecionáveis pela mesma configuração, com
   FIFO como controle. A questão aberta 5 permanece necessária para escolher
   um modelo multimodal compatível em uma SPEC posterior.

## Resoluções registradas pela SPEC-014

ADR-0016 e os contratos locais definem uma porta de apresentação substituível:

1. a referência não escolhe Tauri, Qt ou outro framework, e não abre janela em
   testes;
2. estados visuais são limitados e sempre não bloqueiam trabalho, não capturam
   input e não recebem foco;
3. perguntas/notificações carregam hipótese, confiança, contexto e motivo;
4. feedback permite `correct`, `defer` e `silence`, preservando histórico sem
   declarar emoção, consciência ou personalidade.

## Resoluções registradas pela SPEC-015

O ADR-0017 e os contratos versionados de áudio resolvem a integração mínima:

1. captura, VAD e transcrição são portas locais substituíveis; nenhum backend
   de microfone, codec ou modelo é dependência do núcleo;
2. cada segmento preserva timestamps, confiança de VAD, referência/hash do
   áudio bruto e custo medido;
3. falha de transcrição gera evento de falha e não remove o segmento da
   timeline; ausência de sinal permanece distinta de fala não detectada;
4. a questão 9, sobre retenção do áudio bruto, permanece aberta e é política
   do adaptador local. A SPEC-015 não define retenção nem envia bytes para a
   nuvem.

## Resoluções registradas pela SPEC-016

O ADR-0018 e os contratos de ações supervisionadas resolvem o primeiro gate de
agência:

1. o controlador executa apenas por ActionPort e ActionPolicy abstratos;
2. prepare produz plano e simulação sem executar;
3. authorize exige confirmação explícita, validade temporal e correspondência
   exata de plan_id e plan_digest;
4. execute consome a autorização, audita o resultado e bloqueia a ausência de
   fornecedor; rollback é melhor esforço e também auditado;
5. a SPEC-016 não habilita nenhum atuador concreto nem ações destrutivas.

## Resoluções registradas pela SPEC-019

O ADR-0019 e os contratos de propriocepção resolvem o bloqueio documental:

1. o módulo Python registra estado corporal funcional, intenção, cópia
   eferente, resultado e atribuição;
2. H7 fixa passive_observer_v0 como baseline, macro-F1 como métrica primária,
   ablação pela mesma interface e holdout congelado como critério;
3. own exige correlação de ação e controle com a cópia eferente;
4. ausência de correlação é ambiguous; external exige observação explicitamente
   marcada como externa;
5. promoção para C++ e validade ecológica permanecem fora desta SPEC.

## Resoluções registradas pela SPEC-020

O ADR-0020 e os contratos de consolidação resolvem o bloqueio documental:

1. replay_with_provenance_v1 deriva somente chaves observáveis e registra
   source_episode_ids em todo conhecimento;
2. reconciliação incrementa versões e preserva alternativas/contradições sem
   promover hipóteses a fatos;
3. no_replay_v0 é o baseline pela mesma interface;
4. archive e restore são decisões reversíveis e nenhum episódio é apagado;
5. promoção para C++ e retenção física de bytes permanecem fora desta SPEC.

## Resoluções registradas pela SPEC-021

O ADR-0021 e os contratos versionados de previsão resolvem o incremento de
world model sem criar dependência externa:

1. `frequency_baseline_v0` e `markov_order1_v0` são controles comparáveis para
   `incremental_markov_v1`, que usa contexto n-grama com fallback observável;
2. cada previsão publica uma distribuição de próximos estados, e cada erro
   observado registra log loss, top-k e um fator monotônico de surpresa;
3. uma janela de erro acima do limiar reduz confiança, limpa somente contagens
   derivadas e inicia reaprendizagem, sem apagar eventos ou inventar estados;
4. o fator é mapeado ao sinal de workspace `surprise`; a política de seleção
   permanece no Global Workspace;
5. promoção C++ e validade ecológica permanecem fora desta SPEC.

## Resoluções registradas pela SPEC-022

O ADR-0022 e os contratos de avaliação resolvem a primeira referência
longitudinal:

1. protocolo e holdout são congelados por hashes antes da coleta;
2. snapshots imutáveis cobrem apenas 7, 30 e 90 dias e mantêm métricas
   cognitivas/operacionais separadas;
3. o baseline `chronological_first_snapshot_v0` reporta retenção, calibração,
   ganhos, perdas e ausência de métricas sem convertê-la em zero;
4. cada snapshot preserva versão, digest e fontes do self-model, permitindo
   quantificar drift sem alegar continuidade fenomenal;
5. o relatório é reconstruível por replay local; validade ecológica e promoção
   C++ permanecem fora desta SPEC.
