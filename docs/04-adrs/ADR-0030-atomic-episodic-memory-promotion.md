# ADR-0030: promoção atômica da memória episódica

- Status: accepted
- Data: 2026-07-29
- Decisores: governança do projeto

## Contexto

A referência Python de memória episódica já oferece armazenamento, recuperação
contextual, similaridade e retenção limitada. Isso não autoriza a memória no
runtime C++ nem a confunde com consolidação semântica. A promoção deve manter
proveniência, ausência explícita de embedding e remoção segura.

## Decisão

Promover somente o mecanismo de memória episódica com o identificador
`cognition.episodic_memory.v1`. O candidato C++ recebe episódios já formados,
mantém registros imutáveis por ID, usa embedding somente quando fornecido
localmente e retorna razões/proveniência estruturadas. A consulta sem filtros
usa ordenação cronológica determinística.

Retenção limitada pode remover registros do conjunto ativo conforme orçamento,
mas não cria conhecimento semântico. A entrada da promoção no registry exige
revisão humana identificável; equivalência automatizada classifica o nativo
como `equivalent`, não como produto liberado.

## Consequências

- A memória não depende de modelo, rede, sensor ou Python em runtime.
- Embeddings são sinal opcional, não requisito estrutural.
- Consolidação, fatos, resumos e generalização continuam reservados à SPEC-020.
- Divergências bloqueiam o gate e exigem nova versão de fixture ou especificação.
- O componente pode ser removido sem alterar o núcleo cognitivo.

## Alternativas rejeitadas

- Promover a memória junto com consolidação: mistura contratos e hipóteses.
- Exigir um modelo de embedding: tornaria a capacidade pesada e não removível.
- Tratar a saída Python como ground truth: contradiz a política científica.
