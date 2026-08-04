# ADR-0036 — Backend Ollama restrito ao loopback local

Status: accepted
Date: 2026-08-04
Accepted: 2026-08-04
Decision authority: delegação explícita do responsável humano para o agente
tomar as decisões necessárias do projeto

## Contexto

A ADR-0015 criou uma porta de modelo local sem selecionar runtime ou modelo e
exige ADR própria antes de introduzir um backend concreto. A SPEC-051 propõe o
Ollama, mas a implementação experimental existente aceita proxy do sistema,
não verifica a presença do modelo, interpreta JSON por busca textual e possui
um teste que tolera qualquer falha de rede. Isso não demonstra isolamento,
timeout, cancelamento nem ausência de acesso externo.

O Ollama também oferece modelos cloud por sua API local. Portanto, a mera
restrição do socket ao host local não basta: o modelo permitido precisa ser
vinculado por manifesto a um artefato instalado e validado, e nenhum endpoint
de pull, create, copy, push ou delete pode ser chamado.

## Decisão

1. Ollama é aceito somente como capacidade opcional e removível de inferência
   local, atrás de `LocalModelBackend`. O núcleo cognitivo, o renderer e a UI
   não importam Ollama, WinHTTP ou seu protocolo.
2. A versão 1.0 fixa `http://127.0.0.1:11434`. Hostnames, outros endereços,
   proxy, redirects, TLS, autenticação, API keys e endpoints cloud são
   rejeitados. O transporte WinHTTP usa `NO_PROXY` e redirects desabilitados.
3. O protocolo permitido é somente `GET /api/tags` e
   `POST /api/generate`. Não há download, pull, criação, cópia, publicação ou
   remoção de modelo. `load()` exige correspondência exata de nome, digest,
   tamanho, formato e quantização com um binding local versionado.
4. Configuração e binding do modelo são DTOs 1.0 com schemas normativos em
   `contracts/schemas/`. Campos desconhecidos, versões incompatíveis ou
   valores fora dos limites são rejeitados antes de abrir uma conexão.
5. `OllamaModelBackend` contém serialização e interpretação do protocolo. Um
   `IOllamaTransport` injetável contém apenas a operação HTTP, permitindo
   testes sem processo, porta ou modelo reais. O transporte concreto não é
   dependência estrutural do gateway.
6. A geração é não-streaming, com `stream:false`, `think:false` e
   `keep_alive:0`. O canal separado de thinking não é consumido pelo contrato;
   o modelo é descarregado após cada resposta e o limite de um modelo pesado
   da ADR-0015 é preservado. Corpo, status, tipos JSON, modelo e `done:true`
   são validados antes de produzir `LocalModelRawOutput`; `num_predict` recebe
   um limite explícito da configuração versionada.
7. Timeout e cancelamento fecham a requisição ativa. Corpos têm limites
   explícitos e mensagens de erro não registram prompt nem corpo retornado.
8. O binding inicial validado é `qwen3-vl:2b`, GGUF Q4_K_M, Apache-2.0,
   instalado localmente e abaixo de 4 GiB. O manifesto registra separadamente
   o digest do catálogo Ollama e o SHA-256 do blob GGUF. Nenhum payload é
   incluído no repositório.
9. O backend publica `CapabilityDescriptor`. Ausência, falha, remoção,
   reinstalação ou substituição atualizam o estado de capacidade e mantêm o
   fallback seguro da aplicação.

## Hipótese operacional

`H-SPEC051-LOOPBACK`: um backend com protocolo estrito e transporte injetável
permite inferência local sem tornar Ollama estrutural nem criar uma rota de
rede externa implícita.

- Baseline: `unvalidated_winhttp_backend_v0`.
- Métricas: casos de protocolo rejeitados, timeouts, cancelamentos, bytes de
  resposta e disponibilidade da capacidade.
- Ablação: remover o plugin Ollama e executar o mesmo gateway com backend
  fixture/substituto.
- Teste metamórfico: trocar host, endpoint, digest, tipo JSON ou acrescentar
  bytes após o documento válido deve transformar sucesso em rejeição, sem
  alterar memória ou decisão cognitiva.
- Falsificação: qualquer requisição deixa o loopback, usa proxy/redirect,
  baixa modelo implicitamente, aceita binding divergente, retorna JSON
  inválido ou impede o restante do runtime de operar sem Ollama.

Essas métricas verificam isolamento operacional. Não demonstram compreensão,
aprendizado, consciência ou validade ecológica do conteúdo gerado.

## Consequências

- O produto pode usar um modelo local já instalado, mas continua funcional em
  modo degradado quando configuração, runtime ou modelo estão ausentes.
- Mudança de host, porta, runtime, modelo ou política de rede exige nova versão
  do contrato e revisão arquitetural.
- O Ollama continua sendo um processo local externo ao produto instalado; o
  executável C++ não depende de Python nem envia dados à nuvem.

## Reversão

Desabilitar a configuração ou remover o plugin/backend Ollama. Gateway,
renderer, memória, timeline, contratos cognitivos e fallback permanecem
inalterados.
