---
id: SPEC-034
title: Promoção nativa da memória episódica
status: in_progress
phase: beta
dependencies: [SPEC-008, SPEC-023, SPEC-026, SPEC-027, SPEC-029, SPEC-033]
adrs: [ADR-0005, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0029, ADR-0030]
contracts: [EPISODE_SCHEMA.md, EPISODIC_MEMORY_PROMOTION_CONTRACT.md]
---

# SPEC-034 — Promoção nativa da memória episódica

## Objetivo

Promover o armazenamento e a recuperação local de episódios para uma
implementação C++ nativa verificável. A memória preserva episódios como
registros observados, explica a recuperação e mantém a proveniência dos
eventos.

## Escopo negativo

Não inclui consolidação semântica, criação de fatos, resumo automático,
generalização, padrões, world model, workspace, self-model, metacognição,
modelo local, diálogo, avatar, sensores ou ações.

## Escopo

Inclui armazenamento imutável por `episode_id`, consulta contextual/temporal,
similaridade opcional por embedding local fornecido, relações explícitas,
retenção limitada e reversível no sentido de não apagar a fonte durante a
avaliação. O candidato expõe um `CapabilityDescriptor` removível.

## Dependências e decisões

- `SPEC-008` define a referência Python, baseline cronológico e hipótese.
- `SPEC-033` fornece o contrato e a promoção da segmentação que produz
  episódios; esta SPEC não reimplementa a segmentação.
- `SPEC-020` permanece fora do incremento: sua consolidação semântica terá
  promoção própria.
- `SPEC-026` e `SPEC-027` exigem equivalência, holdout e evidência científica
  independente.
- `ADR-0030` define a promoção atômica e a separação entre candidato e produto.

## Critérios de aceite

- [ ] A referência, o conjunto de equivalência e o holdout possuem hashes
      registrados antes da avaliação final.
- [ ] O candidato armazena episódios válidos, rejeita IDs duplicados sem
      substituir a fonte e mantém o contrato de episódio completo.
- [ ] Consultas por aplicação, documento, modalidade, sessão, tempo e embedding
      opcional reproduzem a referência e retornam razões/proveniência.
- [ ] Relações de similaridade são explícitas e carregam os eventos de origem;
      nenhuma relação vira fato ou resumo.
- [ ] Retenção limitada é determinística e não apaga a semântica da fonte por
      uma operação de consolidação.
- [ ] Existem baseline cronológico, ablação sem contexto/embedding, ground truth,
      invariantes, holdout bloqueado e benchmark p50/p95/máximo.
- [ ] O componente é removível via lifecycle e não torna o produto disponível
      sem promoção aprovada.
- [ ] CTest, suíte Python, equivalência, contratos, documentação e relatório
      passam em Linux e Windows.

## Saída

Um candidato C++ equivalente e auditável para memória episódica. Enquanto a
revisão humana não inserir a promoção no registry, a capacidade permanece
`product_status: unavailable` e o runtime não a trata como disponível.
