# Instruções obrigatórias para agentes

## Missão

Implementar o projeto sem alterar seu objetivo central: criar um agente cognitivo local que aprende padrões por observação contínua, forma memória e constrói modelos do ambiente, do usuário e de si.

## Autoridade documental

Em caso de conflito, siga esta precedência:

1. `docs/00-governance/CONSTITUTION.md`
2. ADRs aceitos
3. contratos em `docs/03-contracts/`
4. SPEC ativa
5. plano de execução
6. código existente
7. comentários e documentação auxiliar

Não resolva conflitos silenciosamente. Registre-os em `docs/05-governance/OPEN_QUESTIONS.md`.

## Proibições

- Não criar tarefas fixas como núcleo do sistema.
- Não transformar o projeto em automação tradicional baseada apenas em regras.
- Não adicionar serviços externos ou APIs sem ADR aprovado.
- Não substituir módulos definidos por um monólito de LLM.
- Não executar ações destrutivas ou irreversíveis.
- Não alterar contratos públicos sem versionamento.
- Não implementar funcionalidades fora da SPEC ativa.
- Não declarar que o sistema possui consciência, emoções reais ou intenção fenomenal.
- Não usar telemetria externa.
- Não enviar dados para a nuvem.

## Procedimento obrigatório por tarefa

1. Leia a SPEC inteira.
2. Liste dependências e contratos.
3. Verifique critérios de entrada.
4. Escreva ou atualize os testes.
5. Implemente o menor incremento suficiente.
6. Execute lint, tipos, testes unitários e integração.
7. Atualize documentação afetada.
8. Gere relatório em `reports/`.
9. Pare se qualquer critério de aceite falhar.

## Definition of Done

Uma SPEC só está concluída quando:

- todos os critérios de aceite passam;
- todos os testes definidos passam;
- nenhuma proibição foi violada;
- interfaces permanecem compatíveis;
- logs e erros são estruturados;
- documentação e exemplos estão atualizados;
- relatório de execução foi criado;
- nenhuma pendência crítica permanece aberta.

## Princípio de menor mudança

Não refatore áreas não relacionadas. Não renomeie conceitos canônicos. Não antecipe fases futuras.

## Regra científica adicional

Toda função cognitiva deve possuir hipótese, baseline, métrica, ablação e critério de falsificação. Consulte `docs/07-research/` antes de implementar módulos cognitivos.

## Regra de independência de capacidades

- O pacote cognitivo central não pode importar implementações concretas de sensores, ferramentas, atuadores ou modelos.
- Toda integração deve publicar um `CapabilityDescriptor`.
- Nenhuma SPEC pode tornar um plugin opcional em dependência estrutural do núcleo.
- Testes devem incluir ausência, falha, remoção, reinstalação e substituição.
- Ausência de observação não pode ser tratada como observação negativa.
- Mudanças de capacidade devem atualizar o self-model e invalidar planos incompatíveis.

## Arquitetura Laboratório/Cérebro

- Python é usado para pesquisa, treinamento, análise e implementações de referência.
- C++ é usado para o runtime instalado.
- O produto final não pode depender de Python.
- Mecanismos cognitivos promovidos devem possuir manifesto de promoção e teste de equivalência.
- Contratos compartilhados pertencem a `contracts/`.
- Não duplicar schemas manualmente em Python e C++.
- Um benchmark melhor não autoriza divergência semântica.
- Código experimental Python não deve ser copiado para C++ antes da validação científica.

## Regra final de validade

- A implementação Python não é ground truth.
- Concordância Python/C++ não prova correção.
- Toda promoção deve distinguir verificação, generalização computacional, validade científica e validade ecológica.
- Testes metamórficos são obrigatórios quando não há oráculo direto.
- Holdouts não podem ser reutilizados como conjunto de desenvolvimento.
- Métricas operacionais não podem ser apresentadas como evidência cognitiva.
