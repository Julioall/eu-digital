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
