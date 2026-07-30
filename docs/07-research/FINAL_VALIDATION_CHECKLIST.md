# Checklist Final de Validação

Use antes de promover qualquer componente cognitivo.

## Hipótese

- [ ] hipótese falsificável;
- [ ] alegação máxima permitida;
- [ ] baseline definido;
- [ ] ablação definida;
- [ ] métricas congeladas;
- [ ] resultado negativo previsto.

## Dados

- [ ] dataset de desenvolvimento;
- [ ] dataset de validação;
- [ ] holdout bloqueado;
- [ ] hashes;
- [ ] proveniência;
- [ ] ground truth ou justificativa de ausência;
- [ ] anotadores independentes quando necessário;
- [ ] análise de drift.

## Referência Python

- [ ] commit congelado;
- [ ] ambiente bloqueado;
- [ ] seeds;
- [ ] configuração;
- [ ] fontes de nondeterminismo;
- [ ] relatório reproduzível.

## Implementação C++

- [ ] contrato compatível;
- [ ] testes unitários;
- [ ] testes metamórficos;
- [ ] equivalência computacional;
- [ ] comparação com ground truth;
- [ ] benchmark;
- [ ] análise de concorrência;
- [ ] teste no hardware-alvo.

## Modelos exportados

- [ ] hash do modelo original;
- [ ] hash do artefato exportado;
- [ ] configuração de exportação;
- [ ] quantização;
- [ ] diferença de acurácia;
- [ ] diferença de calibração;
- [ ] casos divergentes;
- [ ] backend e hardware.

## Sensores e capacidades

- [ ] ausência explícita;
- [ ] falha injetada;
- [ ] degradação;
- [ ] recuperação;
- [ ] substituição;
- [ ] impacto na confiança;
- [ ] impacto no planejamento;
- [ ] identidade preservada.

## Validade ecológica

- [ ] replay;
- [ ] sessão online controlada;
- [ ] ruído;
- [ ] jitter;
- [ ] perda de eventos;
- [ ] drift;
- [ ] sessão longitudinal.

## Decisão

- [ ] revisão independente;
- [ ] divergências publicadas;
- [ ] resultados negativos registrados;
- [ ] promotion manifest;
- [ ] gate aprovado;
- [ ] claim autorizado.
