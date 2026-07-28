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

## Bloqueios arquiteturais da SPEC-011

A SPEC-011 permanece bloqueada até que sejam definidos e aprovados:

1. contratos executáveis e versionados para hipótese, avaliação
   metacognitiva, pergunta estruturada, resposta/correção e supressão, pois
   `HYPOTHESIS_SCHEMA.md` hoje é somente documentação YAML;
2. um protocolo de calibração que relacione previsão, confiança, correção,
   abstinência e custo de interrupção, com baseline, Brier/ECE/AUROC e
   risk–coverage congelados antes do holdout;
3. a política local de orçamento de perguntas, cooldown, redundância e regra
   de silêncio, sem introduzir diálogo, LLM obrigatório, busca externa ou ação
   autônoma;
4. o limite entre a referência Python e eventual promoção C++, que continua
   condicionado à SPEC-026 e à validade independente.

Sem essas decisões, uma implementação escolheria silenciosamente a semântica
de confiança, interrupção e atualização que a SPEC não define.
