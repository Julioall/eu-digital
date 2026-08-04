# Ollama Backend Contract

`ollama_backend_config.schema.json` e `ollama_model_binding.schema.json`, em
`contracts/schemas/`, são as fontes normativas da integração Ollama 1.0.

O backend só pode abrir HTTP sem proxy em `127.0.0.1:11434`, sem redirects. Os
únicos endpoints autorizados são `GET /api/tags` para validar um modelo já
instalado e `POST /api/generate` para uma geração não-streaming. A integração
não chama endpoints de download, pull, criação, publicação ou exclusão e não
aceita modelos cloud.

A geração fixa `stream:false`, `think:false` e `keep_alive:0`: o contrato não
consome o canal separado de thinking, limita `options.num_predict` pela
configuração e libera o modelo pesado após a resposta.

O binding separa:

- o digest e tamanho observados no catálogo do Ollama;
- o SHA-256 e tamanho do blob GGUF local;
- o artefato do runtime e o payload do modelo;
- a compatibilidade operacional da alegação de qualidade cognitiva.

`IOllamaTransport` é injetável e não interpreta o protocolo. Somente
`OllamaModelBackend` serializa requests e valida responses. Ausência ou falha
do transporte/modelo torna a capacidade indisponível; não representa uma
observação negativa sobre o usuário ou o ambiente.

O binding inicial não inclui bytes de modelo e não autoriza download. A
assinatura `detached_manifest_digest_v1` é somente um envelope local de
integridade, conforme o contrato do gateway; não é assinatura assimétrica de
release.
