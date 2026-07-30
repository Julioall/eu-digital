# Constituição do Projeto

Versão: 1.1.0  
Status: normativa

## Artigo 1 — Propósito

O projeto existe para investigar e implementar um agente digital local capaz de construir modelos progressivamente melhores do ambiente, do usuário e de si a partir da experiência.

A unidade central de progresso não é o número de tarefas executadas. É a melhora verificável da capacidade de:

- perceber;
- segmentar experiências;
- recuperar contexto;
- reconhecer padrões;
- prever transições;
- estimar incerteza;
- formular perguntas úteis;
- aprender com correções;
- manter continuidade temporal.

## Artigo 2 — Cognição modular

O sistema será composto por módulos especializados. Nenhum LLM poderá assumir sozinho percepção, memória, aprendizagem, decisão e identidade.

## Artigo 3 — Independência de sensores e ferramentas

O núcleo cognitivo não poderá depender diretamente de um sensor, atuador, ferramenta, modelo ou integração concreta.

Sensores e ferramentas serão capacidades periféricas, removíveis e substituíveis. A ausência, falha, remoção ou adição de uma capacidade deve ser representada explicitamente no modelo de si e tratada por degradação previsível.

O sistema não poderá:

- presumir a presença de uma modalidade;
- inventar observações quando um sensor estiver ausente;
- manter planos dependentes de uma ferramenta indisponível;
- corromper memória ou identidade ao trocar módulos;
- exigir recompilação do núcleo para adicionar uma nova capacidade compatível.

O núcleo deve operar por contratos abstratos, eventos canônicos e descoberta de capacidades.

## Artigo 4 — Localidade

O funcionamento padrão será integralmente local. Dados sensoriais, memórias e modelos pessoais não devem sair da máquina.

## Artigo 5 — Aprendizagem por experiência

O agente não deve nascer com um catálogo de tarefas pessoais. Ele deve formar hipóteses a partir de sequências observadas, testar essas hipóteses e atualizá-las com feedback.

## Artigo 6 — Epistemologia explícita

Toda crença operacional deve registrar, quando aplicável:

- origem;
- evidência;
- confiança;
- recência;
- contradições;
- histórico de atualização.

## Artigo 7 — Autonomia gradual

A progressão será:

1. observar;
2. descrever;
3. perguntar;
4. prever;
5. sugerir;
6. preparar;
7. executar com confirmação;
8. executar dentro de políticas limitadas.

Nenhuma fase pode ser pulada sem ADR e validação.

## Artigo 8 — Segurança operacional

Mesmo em ambiente controlado, o sistema deve possuir limites técnicos, auditoria, reversibilidade e modos de falha previsíveis.

## Artigo 9 — Honestidade ontológica

O projeto pode implementar um eu funcional. Não deve afirmar consciência fenomenal, sentimentos reais ou direitos morais com base apenas em comportamento.

## Artigo 10 — Mudanças constitucionais

Alterações exigem:

- proposta escrita;
- justificativa;
- impacto;
- ADR;
- atualização de versão;
- aprovação humana explícita.

## Artigo 11 — Separação entre laboratório e organismo

O ambiente científico e o runtime instalado possuem responsabilidades diferentes.

Python poderá ser usado para experimentação, treinamento, análise e referência. O Cérebro Implantado será executado em C++ e não dependerá de Python.

Nenhum mecanismo experimental será incorporado ao runtime apenas por plausibilidade. A promoção requer evidência, contrato, equivalência e avaliação operacional.
