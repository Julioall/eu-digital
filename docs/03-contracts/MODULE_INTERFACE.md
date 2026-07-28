# Contrato Padrão de Módulo

Cada módulo deve declarar:

- nome;
- versão;
- entradas;
- saídas;
- estado persistente;
- dependências;
- limites de recurso;
- política de retry;
- política de idempotência;
- erros tipados;
- métricas;
- health check;
- estratégia de migração;
- operações fornecidas;
- operações exigidas;
- dependências opcionais;
- manifesto de capacidade;
- estados de ciclo de vida;
- suporte a hot-plug;
- procedimento de calibração;
- comportamento na ausência de dependências;
- política de checkpoint e restauração;
- compatibilidade de versão.

Toda interface pública deve possuir esquema versionado e testes de contrato.

## Regra de desacoplamento

O núcleo cognitivo só pode consumir este contrato, o registro de capacidades e schemas canônicos. Imports de implementações concretas são proibidos.
