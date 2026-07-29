# Plano 002 — Runtime local mínimo do Cérebro Implantado

SPEC: SPEC-028
Status: proposed
Fase: 0.3

## Objetivo

Implementar o menor host C++ que permita iniciar, observar, testar, persistir
e encerrar o runtime local sem Python, rede ou capacidades de domínio
obrigatórias.

## Sequência

1. Congelar e validar os contratos `RuntimeManifest` e `RuntimeHealth`.
2. Definir a API interna de `RuntimeHost` sem importar plugins concretos.
3. Implementar estados, códigos de erro, logs estruturados e snapshot de saúde.
4. Integrar event bus, capability registry e timeline com diretório configurável.
5. Implementar modos finitos de verificação e replay com relógio controlado.
6. Criar testes unitários, integração, falhas, reinício e empacotamento.
7. Validar Linux/WSL e Windows nativo com toolchains declarados.
8. Atualizar documentação, relatório e manifesto de release.

## Gate de promoção

Nenhum mecanismo cognitivo Python será promovido por este plano. A fase só
termina quando os critérios da SPEC-028 passarem e o pacote instalado não
contiver Python.
