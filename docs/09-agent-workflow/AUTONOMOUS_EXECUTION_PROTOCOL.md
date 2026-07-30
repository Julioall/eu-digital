# Protocolo de Execução Autônoma

## Entrada

Uma única SPEC marcada como `ready`, acompanhada de plano.

## Ciclo

1. Carregar constituição e instruções.
2. Verificar dependências.
3. Criar branch `spec/NNN-slug`.
4. Produzir análise de impacto.
5. Criar ou ajustar testes.
6. Implementar incremento mínimo.
7. Validar contratos.
8. Executar qualidade e testes.
9. Revisar escopo negativo.
10. Gerar relatório.
11. Abrir pull request.
12. Parar.

## Condições de parada obrigatória

- requisito ambíguo que altera arquitetura;
- conflito documental;
- contrato ausente;
- necessidade de serviço externo;
- teste crítico impossível;
- mudança constitucional;
- ação destrutiva;
- dependência não satisfeita.

## Política de interpretação

Quando houver mais de uma implementação válida, escolha a menor, mais reversível e mais testável. Não escolha a mais sofisticada sem exigência.
