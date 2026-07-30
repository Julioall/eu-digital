# ADR-0011 — Verificação não é validação científica

Status: aceito  
Data: 2026-07-28

## Contexto

O projeto possui uma implementação de referência em Python e um runtime C++. Produzir resultados equivalentes entre as duas implementações é necessário, mas pode reproduzir o mesmo erro conceitual ou algorítmico.

## Decisão

Separar formalmente:

1. verificação de implementação;
2. generalização computacional entre implementações;
3. validade contra ground truth;
4. validade ecológica;
5. efeito causal por ablação.

A referência Python é um oráculo de regressão, não ground truth científico.

## Consequências

- fixtures sintéticas com verdade conhecida;
- holdout bloqueado;
- testes metamórficos;
- auditoria de modelos exportados;
- sessões online;
- testes longitudinais;
- relatórios separados para correção e desempenho.

## Rejeitado

- aceitar equivalência Python–C++ como prova suficiente;
- usar somente snapshots de saída;
- ajustar tolerâncias após observar o holdout;
- tratar desempenho como validade cognitiva.
