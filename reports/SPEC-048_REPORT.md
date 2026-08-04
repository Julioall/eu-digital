# Relatório de Execução

SPEC: SPEC-048 — Structured Cognitive Output and Dialogue
Agente: Codex
Data: 2026-08-04
Commit: incluído no commit de conclusão da SPEC-048

## Alterações realizadas

- Aceita a ADR-0035 e definida a saída como observador assíncrono pós-commit,
  sem alterar `CognitiveCycleResult` 1.0.
- Publicados request, candidato do renderer e output validado em schemas 1.0.
- Implementado parser JSON estrito: campos extras/duplicados, tipos, intenção,
  request ID e referências não autorizadas são rejeitados.
- Substituído `std::async` por isolamento com `stop_token`; timeout não aguarda
  destrutor de `future`, e trabalho não cooperativo não acessa o renderer após
  sua destruição.
- Adicionado `CognitiveOutputCoordinator` com worker, fila limitada, supressão
  de duplicata, métricas e logs estruturados.
- Corrigido `CognitiveDecisionAdapter`: resposta explícita é classificada antes
  do `SuggestionOrchestrator`, sem decisão proativa, débito ou cooldown.
- Integrado o request ao final do ciclo live e ao `RuntimeHost`; replay não
  produz request nem UI.
- Registrados no shell um renderer sem backend, limitado ao fallback seguro, e
  um adaptador Qt que entrega na thread da UI sem mudança de QML.

## Arquivos modificados

- contratos e fixtures em `contracts/schemas/` e `contracts/fixtures/`;
- contratos/portas/runtime em `cpp/core/`;
- adaptador Qt e registro desktop em `cpp/shell/`;
- testes C++, Python e configuração CMake;
- ADR-0035, SPEC-048, PLAN-048, contratos documentais e questões abertas.

## Testes executados

- build C++ headless completo em `build/windows-dev`;
- CTest headless completo;
- suíte Python completa;
- testes de schemas e lint do arquivo Python alterado;
- mypy do pacote de laboratório;
- validadores documentais e de SPECs;
- build de `eu_digital_desktop`, `desktop_integration_test` e do adaptador Qt;
- teste Qt do adaptador e execução delimitada da integração desktop;
- microbenchmark embutido de enqueue.

## Resultados

- build headless: sucesso;
- CTest headless: 46/46;
- pytest: 244/244;
- mypy: sucesso em 28 arquivos;
- lint focado: sucesso;
- documentação: 13/13;
- SPECs: 54 válidas;
- Qt adapter: 1/1 e shell/integração compilados;
- integração desktop delimitada: exit code 0;
- mediana de enqueue: 1 µs, abaixo do limite de 1 ms.

## Critérios de aceite

- [x] `ILanguageRenderer` e `IPresentationPort` existem e são substituíveis.
- [x] Resposta solicitada não muta orçamento, decisões ou cooldown proativo.
- [x] Timeout retorna fallback/silêncio sem bloquear o coordenador.
- [x] Todo output do renderer satisfaz o contrato 1.0 estrito.

## Desvios

O lint global foi executado e encontrou 40 violações preexistentes fora dos
arquivos da SPEC (imports, `Optional`, `timezone.utc` e no-ops de noqa). O único
arquivo Python alterado pela SPEC passa isoladamente; os 40 itens não foram
modificados para respeitar o princípio de menor mudança.

## Riscos e pendências

- O renderer de produção continua sem backend/modelo selecionado, conforme
  ADR-0015 e a questão arquitetural 20 da SPEC-051.
- Validação estrutural limita formato e proveniência, mas não prova veracidade
  ou qualidade semântica; isso requer avaliação própria.
- Uma implementação arbitrária de `IPresentationPort` ainda deve cumprir o
  contrato não bloqueante da ADR-0016.

Nenhuma pendência é crítica para os critérios da SPEC-048.

## Decisões tomadas

- `contracts/schemas/` é a fonte normativa; a pasta raiz `schemas/` é legado.
- Falha crítica usa frase fixa de indisponibilidade; falha proativa silencia.
- Capacidades são resolvidas por request para permitir remoção, reinstalação e
  substituição sem dependência estrutural.
- Nenhuma implementação Ollama/HTTP foi promovida ou usada pelo pipeline.

## Evidências

- `cognitive_output_enqueue_median_us=1`;
- testes de timeout cooperativo e não cooperativo;
- testes de campo extra, JSON truncado, evidência externa e ID divergente;
- testes de ausência/falha/remoção/reinstalação/substituição das duas portas;
- teste de replay com zero requests de saída;
- teste de identidade do estado do `SuggestionOrchestrator` antes/depois de
  respostas explícitas.
