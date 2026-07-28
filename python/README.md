# Laboratório Python

Este diretório contém protótipos, treinamento, análise, implementações de referência e experimentos.

Código experimental não entra automaticamente no Cérebro Implantado. A promoção segue a SPEC-026.

O sandbox reprodutível da SPEC-017 está em `eu_digital_lab/` e usa somente a
biblioteca padrão. Execute `python -m unittest discover -s python/tests -v` para
validar reprodutibilidade, ground truth, splits e anotações humanas.

Para validar o pacote isolado e a fixture compartilhada, use
`uv run python tools/validate_hybrid.py`.

The SPEC-002 in-process asynchronous bus is in `eu_digital_lab/event_bus.py`.
It validates `CanonicalEvent` with the shared schema, applies bounded-queue
backpressure, deduplicates `event_id`, and retains rejected events locally.
The Python-free C++ runtime equivalent is `cpp/core/event_bus.hpp`.
