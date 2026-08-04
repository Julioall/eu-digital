# Plano de Correção: SPEC-053 — Companheiro Local de Atividades

Status: bloqueado por governança e contratos

SPEC candidata: `SPEC-053`

Escopo: corrigir a especificação e o vertical slice já existente, sem ampliar
funcionalidades nem antecipar outra SPEC.

## Objetivo

Tornar a SPEC-053 apta para implementação e validação, alinhando documentação,
contratos compartilhados, consentimento, ciclo cognitivo, capacidades removíveis
e interface Qt. O plano não considera o código já existente como evidência de
conclusão.

## Impacto

A correção alcança somente as fronteiras necessárias ao vertical slice da
SPEC-053:

- governança da SPEC e das dependências 045 e 047;
- contratos de atividade, assistência e resultado do ciclo;
- composição do runtime desktop e `CognitiveCoordinator`;
- consentimento e lifecycle dos sensores;
- apresentação Qt da atividade e dos cards;
- backend local opcional atrás da porta da SPEC-040;
- testes, documentação e relatório da própria SPEC-053.

Não entram neste plano câmera concreta, novo modelo, download de artefatos,
telemetria, ação autônoma, nova memória, nova política cognitiva ou mudança do
`CanonicalEvent` 1.0.

## Bloqueios de entrada

1. A SPEC-053 está em `draft` e não foi indicada como ativa/aprovada.
2. As dependências SPEC-045 e SPEC-047 também permanecem em `draft`, apesar de
   haver implementação no histórico.
3. O frontmatter da SPEC-053 declara `contracts: []`, embora o corpo introduza
   `CurrentActivity` e `ContextualAssistanceCard`.
4. `CognitiveCycleResult`, usado na fronteira EventBus/UI, também não possui
   schema compartilhado e versionado.
5. Os critérios de aceite aparecem duas vezes e se contradizem: cinco estão
   previamente marcados como concluídos e um critério genérico permanece
   pendente.
6. Não há hipótese, baseline, métrica, ablação nem critério de falsificação
   para inferência de atividade e seleção de assistência.
7. O backend Ollama concreto não pode tornar-se dependência estrutural sem a
   decisão exigida pelo ADR-0015. A ausência de modelo deve continuar explícita.
8. O critério que menciona câmera/captura de tela não declara a SPEC-032 e não
   pode autorizar uma capacidade concreta por implicação.
9. O build C++ limpo falha antes dos testes porque
   `cpp/core/capability_runtime.hpp` usa `std::shared_ptr` sem incluir
   `<memory>`.

Nenhuma etapa de implementação funcional começa enquanto os itens 1–8 não
forem resolvidos e aprovados documentalmente.

## Etapa 0 — Regularizar a autoridade documental

1. Obter confirmação humana de que a SPEC atribuída é a SPEC-053.
2. Resolver em `OPEN_QUESTIONS.md` o status das SPECs 045–050 com evidência, sem
   inferir `done` a partir de commits.
3. Reescrever a SPEC-053 com um único objetivo, escopo negativo explícito e uma
   única lista de critérios mensuráveis, inicialmente desmarcados.
4. Declarar dependências reais. No mínimo: SPEC-030, SPEC-032 quando captura de
   tela fizer parte do aceite, SPEC-040 para a porta de modelo, SPEC-045,
   SPEC-047 e as fronteiras de apresentação já aprovadas.
5. Manter o modelo concreto fora do requisito estrutural. Se Ollama for
   requisito de produto, criar e aprovar ADR próprio antes da implementação;
   caso contrário, tratá-lo somente como adaptador opcional local.
6. Alterar o status da SPEC para `ready` ou `in_progress` somente após aprovação
   humana e satisfação das dependências.

### Gate 0

- SPEC única e inequivocamente atribuída;
- dependências com status coerente e evidência registrada;
- ADRs e contratos listados no frontmatter;
- nenhum conflito crítico aberto sobre o escopo da SPEC-053.

## Etapa 1 — Versionar os contratos antes do código

Criar como fontes de verdade em `contracts/schemas/`:

- `current_activity.schema.json`;
- `contextual_assistance_card.schema.json`;
- `cognitive_cycle_result.schema.json`.

Adicionar documentação semântica em
`docs/03-contracts/ACTIVITY_COMPANION_SCHEMA.md` e fixtures válidas/inválidas.
Os schemas devem definir versão, IDs, timestamps, proveniência, evidência,
confiança/relevância limitadas a `[0, 1]`, ausência explícita e enums fechados.
Texto descritivo deve ser uma projeção de evidência, não um fato inventado.

Os DTOs C++ devem respeitar os schemas compartilhados nas fronteiras de
serialização. Não duplicar um segundo schema manual no código e não alterar o
`CanonicalEvent` 1.0.

### Gate 1

- fixtures válidas aceitas e inválidas rejeitadas;
- round-trip C++/JSON preserva todos os campos;
- caracteres especiais são serializados por biblioteca JSON, nunca por
  concatenação de strings;
- contratos ausentes ou incompatíveis geram erro estruturado.

## Etapa 2 — Congelar o protocolo científico mínimo

A inferência de atividade e a seleção de assistência são funções cognitivas e
precisam de protocolo antes de ajuste de comportamento:

- hipótese: contexto temporal observado melhora a identificação da atividade e
  a relevância da assistência sem elevar interrupções injustificadas;
- baseline: última aplicação observada, sem memória nem inferência contextual;
- métricas cognitivas: macro-F1/accuracy da atividade, calibração da confiança,
  precisão da assistência e ganho por sugestão confirmada;
- métricas operacionais separadas: latência, CPU, RAM e interrupções por hora;
- ablação: remover memória/contexto e executar pela mesma porta;
- falsificação: o tratamento não supera o baseline no holdout, piora
  calibração, ou aumenta interrupções sem ganho confirmado;
- metamórficos: deslocamento uniforme de timestamps, renomeação de IDs opacos,
  duplicação de evento, remoção de modalidade irrelevante e redução de qualidade
  do sensor.

O holdout deve ser separado do desenvolvimento e congelado antes da avaliação.
Resultados operacionais não poderão ser descritos como evidência cognitiva.

### Gate 2

- hipótese, baseline, métricas, ablação e falsificação aprovados;
- dataset de desenvolvimento e holdout distintos, com hashes;
- alegação máxima limitada a assistência contextual operacional.

## Etapa 3 — Escrever os testes corretivos primeiro

### Documentação e contrato

- validar frontmatter, dependências, ADRs e contratos da SPEC-053;
- validar os três schemas e fixtures positivas/negativas;
- rejeitar IDs vazios, timestamps inválidos, enums desconhecidos, scores fora
  do intervalo e campos adicionais não versionados.

### Runtime e privacidade

- primeira execução sem consentimento não instancia nem inicia sensores;
- concessão vale por `sensor_id` e finalidade por meio do `ConsentLedger`;
- revogação e pausa bloqueiam captura antes da criação do evento;
- controle individual realmente pausa/retoma a capacidade selecionada;
- ausência, falha, remoção, reinstalação e substituição atualizam registry e
  self-model sem inventar observação;
- planos dependentes de capacidade removida são invalidados;
- ausência de modelo produz degradação explícita e não bloqueia o runtime.

### Ciclo e UI

- toda entrada de usuário é um `CanonicalEvent` válido e passa pelo
  `CognitiveCoordinator`;
- a UI não chama backend de modelo concreto;
- `CurrentActivity` e `ContextualAssistanceCard` percorrem o fluxo tipado de
  ponta a ponta, sem serem substituídos por strings soltas;
- resposta solicitada usa `ILanguageRenderer`/gateway opcional ou retorna falha
  estruturada; nenhum placeholder é apresentado como resposta real;
- resultado com JSON inválido, timeout ou campo ausente degrada sem travar a UI;
- sinais cross-thread usam conexão enfileirada e encerramento não causa
  deadlock;
- repetição do mesmo evento/card respeita idempotência.

### Backend local

- substituir o teste que aceita qualquer exceção do Ollama por transporte HTTP
  mockado que verifique serialização, resposta, timeout e cancelamento;
- provar que o teste não depende de serviço Ollama em execução nem acessa rede
  externa;
- provar substituição e ausência do backend pela mesma porta.

### Desempenho

- medir latência do roteamento cognitivo separadamente da inferência pesada;
- medir responsividade do tray, idle CPU e shutdown;
- registrar hardware, compilador, backend e limites; não usar desempenho como
  prova cognitiva.

## Etapa 4 — Aplicar o menor conjunto de correções

Somente após os Gates 0–2:

1. restaurar compilação autossuficiente de `capability_runtime.hpp`, começando
   pelo include direto de `<memory>`;
2. substituir o booleano global em `QSettings` pela decisão versionada do
   `ConsentLedger`, mantendo apenas preferência visual fora do ledger;
3. impedir a criação/inicialização de adaptadores antes da autorização;
4. implementar pause/resume/revoke por lifecycle/registry; remover callbacks
   sem efeito;
5. usar os DTOs versionados no coordenador e na UI;
6. serializar resultados com JSON seguro e preencher os campos obrigatórios de
   `CanonicalEvent`;
7. remover `catch (...)` silenciosos nas fronteiras e publicar erros locais
   estruturados, sem conteúdo sensível;
8. substituir o texto placeholder por renderer opcional ou silêncio/degradação
   explícita;
9. manter composição concreta apenas no composition root desktop; o núcleo
   continua dependente exclusivamente de portas;
10. preservar ausência de telemetria, nuvem e Python no produto instalado.

## Etapa 5 — Validação obrigatória

Executar, nesta ordem:

```text
powershell -NoProfile -ExecutionPolicy Bypass -File tests/documentation/Test-Documentation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate_documentation.ps1
python3 tools/validate_contracts.py
PYTHONPATH=python python3 -m unittest discover -s python/tests -v
cmake --fresh --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev
cmake --fresh --preset windows-qt
cmake --build --preset windows-qt
ctest --preset windows-qt
```

Também executar lint, tipos, teste de pacote sem Python e os testes científicos
definidos na Etapa 3. O CMake no WSL deve usar build temporário separado para
não reutilizar cache criado no caminho Windows.

## Critérios de parada

Parar imediatamente se ocorrer qualquer um destes casos:

- SPEC ou dependência ainda não aprovada;
- ADR necessário ausente;
- contrato público sem schema/versionamento;
- sensor iniciado sem consentimento resolvido;
- controle de sensor apenas visual, sem efeito no lifecycle;
- backend concreto torna-se dependência do núcleo;
- ausência vira observação negativa;
- teste exige serviço externo ou rede;
- build, teste unitário, integração, metamórfico ou aceite falha;
- alteração ultrapassa o escopo da SPEC-053.

## Rollback

Desabilitar a composição da SPEC-053 no entrypoint desktop e retornar à
apresentação passiva aprovada, sem apagar timeline, consentimentos, memória ou
histórico de capacidades. Nenhuma tabela ou dado persistido deve ser removido.

## Evidências de conclusão esperadas

- SPEC-053 aprovada e sem critérios contraditórios;
- contratos e fixtures versionados;
- protocolo científico congelado;
- build e CTest completos em Windows e build de portabilidade no WSL;
- testes de consentimento, capacidades, ciclo e UI aprovados;
- relatório em `reports/` baseado em `templates/EXECUTION_REPORT.md`;
- nenhuma pendência crítica aberta.
