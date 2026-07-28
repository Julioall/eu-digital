# ADR-0014 — Modelo de si funcional versionado e causal

Status: aceito
Data: 2026-07-28
Decisores: aprovação humana explícita

## Contexto

A SPEC-012 exige que capacidades, limitações, estado e história alterem
decisões de forma verificável. O contrato `self_model.schema.json` da
SPEC-023 descreve uma projeção atual de capacidades; alterá-lo quebraria um
contrato público já consumido pelo runtime de plugins. Ele também não define
snapshots imutáveis, evidência epistêmica, eventos internos ou uma decisão
causal para um orquestrador.

A matriz de evidência classifica self-model operacional como B, de confiança
média. Isso permite uma referência local, mas requer baseline, ablação e
falsificação, sem alegar subjetividade ou consciência.

## Opções consideradas

1. Alterar o contrato público da SPEC-023 sem versionamento.
2. Manter apenas uma projeção mutável e decorativa para explicações.
3. Criar snapshots versionados complementares, eventos internos tipados e uma
   porta de decisão que consulte o snapshot.
4. Delegar identidade, limitações e decisões a diálogo ou a um LLM.

## Decisão

Adotar a opção 3 para a SPEC-012.

- `self_model.schema.json` permanece compatível e não é modificado.
- Novos contratos versionados representam evento interno, snapshot funcional e
  decisão do orquestrador. Uma atualização produz um novo snapshot imutável
  ligado ao anterior e preserva versões recuperáveis.
- Afirmações são classificadas como `fact`, `hypothesis` ou `configuration`.
  Ausência de uma capacidade é `unverified`, não prova de indisponibilidade.
- A política `self_model_gate_v1` permite uma decisão somente quando a versão
  atual declara a capacidade como disponível. Estados degradado, indisponível,
  removido e não verificado geram explicação estruturada, sem executar ação.
- `unconstrained_decision_v0` é o baseline removível para ablação. A referência
  Python é local, efêmera e independente de sensores, ferramentas, atuadores,
  plugins concretos e LLMs.
- Promoção para C++ exige SPEC-026 e validação independente conforme ADR-0010
  e ADR-0011.

## Consequências

Positivas:

- uma mudança de capacidade altera uma versão auditável e pode bloquear uma
  decisão incompatível;
- explicações distinguem limitação declarada de capacidade ainda não
  verificada;
- a ablação demonstra se o modelo realmente altera decisões, em vez de servir
  apenas como texto descritivo;
- fatos, hipóteses e configuração não são confundidos.

Custos e limites:

- o histórico em memória não substitui persistência longitudinal futura;
- a decisão é uma autorização estrutural, não uma execução de ferramenta;
- métricas locais não estabelecem self fenomenal, personalidade ou validade
  ecológica.

## Plano de reversão

Selecionar `unconstrained_decision_v0` no experimento ou remover o chamador
da referência. Como snapshots e eventos são contratos complementares, a
remoção não altera os contratos de evento canônico, capability runtime ou
self-model legado.
