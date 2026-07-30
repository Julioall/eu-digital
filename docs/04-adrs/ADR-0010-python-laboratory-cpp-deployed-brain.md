# ADR-0010 — Laboratório Python e Cérebro Implantado C++

Status: aceito  
Data: 2026-07-28

## Contexto

O projeto precisa combinar duas necessidades conflitantes:

1. alterar e testar mecanismos cognitivos com rapidez;
2. produzir um agente local permanente, otimizado, instalável e sem dependências frágeis.

Python maximiza a velocidade científica e o acesso ao ecossistema de aprendizagem de máquina. C++ oferece maior controle do runtime, integração nativa, previsibilidade e independência de interpretador.

Usar somente Python prejudicaria o objetivo de runtime final de máximo desempenho. Usar somente C++ reduziria severamente a velocidade de investigação.

## Decisão

Adotar um monorepositório com dois ambientes:

- **Laboratório Python:** protótipos, treinamento, análise, implementações de referência e validação;
- **Cérebro Implantado C++:** produto final instalável e runtime permanente.

Python não será dependência do produto instalado.

A transferência entre os ambientes ocorrerá por contratos, datasets, modelos exportados, fixtures e testes de equivalência.

## Consequências positivas

- experimentação rápida;
- runtime final nativo;
- ausência de Python no instalador;
- rastreabilidade científica;
- comparação objetiva entre protótipo e produção;
- possibilidade de substituir algoritmos sem comprometer o core;
- aproveitamento simultâneo dos ecossistemas Python e C++.

## Custos

- algumas implementações existirão duas vezes;
- criação e manutenção de testes de equivalência;
- disciplina de contratos;
- processo explícito de promoção;
- necessidade de especialistas em duas linguagens;
- maior infraestrutura de build e CI.

## Alternativas rejeitadas

### Somente Python

Rejeitada como arquitetura final devido a desempenho, empacotamento, integração nativa e previsibilidade operacional.

### Somente C++

Rejeitada como ambiente científico exclusivo devido ao custo de prototipação, treinamento e análise.

### Python incorporado ao processo C++

Rejeitada como padrão porque mantém o interpretador como dependência do produto, aumenta acoplamento e reduz isolamento.

### Dois repositórios independentes

Rejeitada inicialmente devido ao risco de divergência entre contratos, datasets e versões.

## Regra normativa

```text
Python é o laboratório.
C++ é o cérebro implantado.
Contratos e evidências pertencem aos dois.
```
