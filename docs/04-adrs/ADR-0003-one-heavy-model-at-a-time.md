# ADR-0003 — Um modelo pesado por vez

Status: aceito

## Contexto

A infraestrutura inicial possui CPU Intel Core i7 de 12ª geração e cerca de 16 GB de RAM, sem GPU dedicada confirmada.

## Decisão

Somente um modelo pesado pode permanecer carregado ou executar inferência por vez. OCR, embeddings e aprendizagem incremental devem usar componentes leves.

## Consequências

O gateway de modelos deve controlar fila, descarregamento, prioridades e timeouts.
