# ADR-0035 — Pipeline estruturado e assíncrono de saída cognitiva

Status: accepted  
Date: 2026-08-04  
Accepted: 2026-08-04  
Decision authority: aprovação humana explícita dos DTOs versionados e
delegação para o agente tomar as decisões necessárias do projeto

## Contexto

A SPEC-048 separa a decisão de falar, a renderização linguística e a
apresentação. A implementação preliminar não satisfaz essa fronteira: fabrica
evidência a partir de um evento reduzido, aceita texto livre como se fosse uma
resposta validada e usa `std::async` de modo que o destrutor do `future` pode
bloquear depois do timeout. A resposta explícita do usuário também passa pelo
`SuggestionOrchestrator` antes de ser classificada, debitando orçamento e
alterando cooldown proativo.

`CognitiveCycleResult` 1.0 proíbe texto e estado de UI. ADR-0015 proíbe escolher
implicitamente um modelo ou runtime concreto, e ADR-0016 exige que a
apresentação seja uma capacidade removível e não bloqueante.

A `EVIDENCE_TO_ARCHITECTURE_MATRIX.md` classifica “LLM como interface” como
evidência C e exige baseline sem LLM. Por isso a geração permanece capacidade
opcional, reversível por feature flag, e seu texto nunca conta como evidência
do funcionamento cognitivo.

## Decisão

1. `CognitiveCycleResult` 1.0 permanece inalterado. Após o commit de um ciclo
   live, o coordenador pode publicar um `CognitiveOutputRequest` 1.0 para um
   observador opcional. Replay nunca publica esse request.
2. O request é derivado apenas da decisão validada e dos DTOs versionados já
   produzidos no ciclo: entrada, episódio, memórias referenciadas, workspace,
   metacognição, self-model e decisão. A camada de saída não reavalia
   saliência, não fabrica hipótese e não debita orçamento.
3. `requested_response` e `question` são críticos. Uma resposta explícita do
   usuário é classificada antes do `SuggestionOrchestrator`, preservando
   orçamento, histórico de decisões e cooldown proativos.
4. `ILanguageRenderer` e `IPresentationPort` são portas resolvidas por operação
   no `CapabilityRegistry`. Ausência, falha, remoção ou substituição não muda o
   núcleo nem é tratada como observação negativa.
5. O roteador de saída usa fila limitada e worker próprio. O callback do ciclo
   apenas valida/copia um request e retorna; geração linguística nunca executa
   na thread do coordenador.
6. O renderer recebe uma função local injetada e não importa Ollama, HTTP,
   nuvem ou uma implementação concreta de modelo. O prompt contém o request
   canônico, as restrições do self-model e exige um candidato JSON 1.0.
7. O candidato aceita exatamente `schema_version`, `request_id`, `intent`,
   `rendered_text` e `evidence_refs`. Campos extras, duplicados, tipos errados,
   texto vazio, IDs divergentes ou referências fora do request são rejeitados.
8. Timeout solicita cancelamento cooperativo e devolve imediatamente. Trabalho
   não cooperativo permanece isolado em estado compartilhado, sem acesso ao
   renderer destruído; enquanto ele existir, uma segunda inferência é negada.
9. Falha em resposta crítica produz uma frase local fixa de indisponibilidade,
   marcada `fallback_used`. Falha proativa produz `silence`. O fallback comunica
   falha operacional e não inventa conteúdo factual.
10. Somente `rendered` ou `fallback_used` válido chega à apresentação. Todos os
    resultados do renderer satisfazem `cognitive_output.schema.json`, inclusive
    silêncio e erros absorvidos.
11. `contracts/schemas/` é a fonte normativa dos schemas executáveis. Markdown
    em `docs/03-contracts/` explica sem duplicar schemas; `schemas/` na raiz é
    legado e não recebe cópia manual.

## Hipótese operacional

`H-SPEC048-OUTPUT`: isolar geração e validar sua saída preserva a latência e o
estado decisório do ciclo, impedindo que texto não autorizado alcance a UI.

- Baseline: `synchronous_unvalidated_output_v0`.
- Métricas: latência de enqueue, timeouts, candidatos inválidos, fallbacks,
  silêncios, apresentações e variação do orçamento/cooldown proativo.
- Ablação: remover renderer ou presentation port mantendo o mesmo ciclo.
- Teste metamórfico: acrescentar campo desconhecido ou referência não contida
  no request transforma a saída em fallback/silêncio, nunca em apresentação do
  texto candidato.
- Falsificação: enqueue mediano acima de 1 ms, thread cognitiva bloqueada pelo
  modelo, texto inválido apresentado, replay produzindo UI ou resposta
  solicitada alterando qualquer estado proativo.

São métricas operacionais de isolamento e segurança de formato. Não demonstram
compreensão, consciência, emoção ou validade cognitiva do texto.

## Consequências

- Um produto sem renderer ou sem superfície continua processando memória e
  decisão e registra indisponibilidade estruturada.
- Um backend local futuro pode implementar a porta sem mudar o ciclo. Sua
  seleção e promoção continuam sujeitas à ADR-0015 e à SPEC apropriada.
- O limite não garante veracidade factual; ele restringe formato, identidade do
  request e referências. Qualidade semântica exige avaliação separada.

## Reversão

Desabilitar o coordenador de saída ou remover suas capacidades. Nenhum dado de
memória, snapshot, timeline ou contrato do ciclo precisa ser migrado ou apagado.
