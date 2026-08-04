# Relatório de Execução

SPEC: SPEC-051 — Ollama Backend Integration
Agente: Codex
Data: 2026-08-04
Commit: incluído no commit de conclusão da SPEC-051

## Alterações realizadas

- Aceita a ADR-0036 e encerrada a questão aberta 20, autorizando Ollama somente
  como capacidade local, opcional e removível.
- Publicados `OllamaBackendConfig` 1.0 e `OllamaModelBinding` 1.0 em schemas
  normativos, com configuração estrita de loopback e binding do modelo.
- Vinculado o `qwen3-vl:2b` instalado: catálogo Ollama, GGUF Q4_K_M, blob de
  1.889.496.384 bytes, SHA-256 verificado e licença Apache-2.0.
- Reescrito `OllamaModelBackend` com `IOllamaTransport` injetável, parser JSON
  estrito, validação UTF-8/surrogates, catálogo local e resposta estruturada.
- Implementado transporte WinHTTP sem proxy e com redirects desabilitados,
  limites de corpo, status HTTP, timeout e cancelamento por fechamento do
  request ativo.
- Limitado o protocolo a `GET /api/tags` e `POST /api/generate`, sem pull ou
  outro endpoint mutável. A geração fixa `stream:false`, `think:false` e
  `keep_alive:0`, com `num_predict` limitado a 512 tokens.
- Publicado `CapabilityDescriptor` do backend e coberto seu lifecycle completo.
- Removido `OllamaDialogueService`, caminho antigo que importava o backend
  concreto diretamente no shell e ignorava o pipeline estruturado.
- Criado `ollama_live_probe`, fora do CTest, para validação explícita contra o
  runtime/modelo local sem imprimir prompt nem texto retornado.

## Arquivos modificados

- ADR, contrato e governança: `docs/04-adrs/ADR-0036-ollama-loopback-backend.md`,
  `docs/03-contracts/OLLAMA_BACKEND_CONTRACT.md` e
  `docs/05-governance/OPEN_QUESTIONS.md`;
- schemas/configuração/binding em `contracts/schemas/`, `config/` e
  `models/manifests/`;
- backend, transporte e parser em `cpp/core/adapters/`;
- probe em `cpp/app/ollama_live_probe.cpp`;
- testes C++ e Python, configuração CMake e a própria SPEC;
- removido `cpp/shell/ollama_dialogue_service.hpp`.

## Testes executados

- build C++ Release completo;
- CTest completo;
- suíte Python completa;
- mypy do pacote de laboratório;
- Ruff focado no arquivo Python alterado e auditoria Ruff global;
- validação de SPECs, configuração normativa e `git diff --check`;
- build Qt de desktop, integração e adapters;
- execução direta dos dois testes Qt e da integração desktop com runtimes LLVM
  e Qt explicitamente ordenados no `PATH`;
- probe C++ real contra `127.0.0.1:11434` e `qwen3-vl:2b`.

## Resultados

- build C++: sucesso;
- CTest: 46/46;
- pytest: 247/247;
- mypy: sucesso em 28 arquivos;
- Ruff focado: sucesso;
- SPECs: 54 válidas;
- configuração normativa: válida;
- Qt shell: 3/3 invariantes, adapter de diálogo exit 0 e integração desktop
  exit 0;
- probe local: sucesso, resposta com 2 bytes, cold starts observados entre
  63,8 s e 75,7 s;
- teste de timeout total: cancelamento estruturado aos 30 s, sem modelo retido.

## Critérios de aceite

- [x] Backend real envia e interpreta JSON da API Ollama local.
- [x] HTTP e protocolo permanecem no pacote do adapter, atrás de porta injetada.
- [x] Mock cobre serialização, parsing, timeout, falha e cancelamento.
- [x] Catálogo divergente/ausente é rejeitado sem pull.
- [x] Loopback, endpoint, proxy, redirect e limites são restritos.
- [x] JSON/UTF-8 truncado, malformado, duplicado ou tipado incorretamente é
  rejeitado.
- [x] Gateway mantém worker/modelo pesado único e descarrega após a resposta.
- [x] Lifecycle cobre ausência, falha, remoção, reinstalação e substituição.
- [x] Ausência ou configuração desabilitada preserva fallback e demais
  capacidades.

## Desvios

- A auditoria Ruff global continua com 40 achados preexistentes em arquivos não
  alterados pela SPEC. O novo teste passa isoladamente; os itens globais não
  foram corrigidos para respeitar o princípio de menor mudança.
- O build global ainda mostra um warning preexistente em
  `cpp/core/privacy_storage.hpp:249`, fora do escopo da SPEC.
- A integração desktop retorna exit 0, mas emite no shutdown o diagnóstico
  preexistente `event bus is unavailable`; o backend Ollama não é compilado no
  shell e não participa desse caminho.

## Riscos e pendências

- Cold start do modelo instalado leva cerca de um minuto nesta máquina. É custo
  operacional do modelo/runtime, não evidência de qualidade cognitiva.
- O artefato instalado é uma variante Thinking e, no Ollama 0.32.5, ainda pode
  preencher `thinking` mesmo com `think:false`. O backend nunca promove esse
  campo a resposta; limita a geração a 512 tokens e rejeita resposta final
  vazia, acionando o fallback.
- O binding prova identidade e integridade observada durante esta execução; o
  repositório não inclui o payload e não o baixa. Remoção ou corrupção futura
  torna a capacidade indisponível ou falha na geração.
- A SPEC-051 entrega backend + gateway validado. A composição do renderer de
  produção continua separada para não reabrir SPECs já encerradas nem contornar
  o pipeline estruturado.

Nenhuma pendência crítica permanece nos critérios da SPEC-051.

## Decisões tomadas

- Endpoint 1.0 fixo em `127.0.0.1:11434`, sem hostname configurável.
- `qwen3-vl:2b` é o primeiro binding operacional; ele não é ground truth nem
  prova científica de cognição.
- Respostas aceitam campos documentados adicionais do Ollama, mas exigem
  `model`, `response` textual não vazio e `done:true`.
- Thinking separado é solicitado como desabilitado porque o contrato não o
  consome; o orçamento continua limitado para runtimes/modelos que ignorem o
  switch.
- O probe usa o próprio `LocalModelGateway`, garantindo timeout total e
  cancelamento cooperativo também na validação live.

## Evidências

- catálogo local: digest
  `0635d9d857d497aeadba3d7d27485746c50554446f9f6ec01ef39788221adbe8`;
- blob GGUF: SHA-256
  `ebabfa59b71a5b96e0281ec2994977e785284e0939807a99fc340dec3c6f10de`;
- probe: `ollama_live_probe: ok; response_bytes=2`;
- testes metamórficos de endpoint proibido, campo duplicado, trailing bytes,
  UTF-8 inválido, surrogate incompleto, digest/tamanho/quantização divergentes;
- testes de status 404, body limit, timeout, falha e cancelamento;
- testes de gateway real sobre mock e lifecycle completo da capacidade.
