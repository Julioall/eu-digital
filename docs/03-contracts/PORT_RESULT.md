# Contrato: PortResult 1.0

A definição executável está em
`contracts/schemas/port_result.schema.json`.

`PortResult<T>` representa o resultado operacional de uma chamada através de
uma porta cognitiva. Sucesso contém exatamente um valor. Falha não contém valor
e inclui um `PortError` tipado com operação, código, mensagem e indicação de
retry.

O envelope diferencia falha de integração de um resultado válido do domínio.
Por exemplo, um `CognitiveDecision` válido ainda pode escolher silêncio, e um
`PortResult<CognitiveDecision>` falho significa que a porta não conseguiu
produzir decisão alguma.

## Compatibilidade

- `schema_version` permanece `1.0` para esta estrutura.
- novos códigos de erro podem ser adicionados sem alterar a estrutura;
- remoção ou mudança semântica de campos exige nova versão;
- as operações seguras `*_result()` são aditivas e não removem as assinaturas
  legadas das portas C++ durante a migração da SPEC-045.

## Códigos iniciais

- `adapter_delegation_error`: a implementação lançou `std::exception`;
- `unknown_adapter_delegation_error`: a implementação lançou uma falha não
  derivada de `std::exception`.

Falhas não podem ser convertidas em DTOs vazios ou observações negativas.
