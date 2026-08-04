# Arquitetura de Referência

## Estilo

Arquitetura modular orientada a eventos, executada localmente, com persistência e processamento assíncrono.

## Camadas

### 1. Sensores

- captura de tela;
- OCR;
- árvore de acessibilidade;
- janelas e processos;
- teclado e mouse;
- clipboard;
- arquivos;
- áudio;
- câmera futura.

### 2. Ingestão

Recebe sinais, aplica timestamp monotônico e de parede, identifica fonte e registra qualidade.

### 3. Normalização

Converte sinais heterogêneos em `CanonicalEvent`.

### 4. Timeline

Mantém sequência ordenada, permite correlação e consulta temporal.

A timeline SQLite é a fonte da verdade da continuidade cognitiva. Checkpoints
2.0 protegidos localmente aceleram a inicialização, mas só são aceitos quando o
checksum, fingerprint, expiração, cursor e conjunto completo de providers
coincidem. O runtime tenta os dois registros mais recentes e, se necessário,
executa replay integral sem decisão nem efeito externo.

### 5. Segmentação

Agrupa eventos em episódios usando limites temporais, mudança de contexto, atividade e coerência semântica.

### 6. Memória

- episódica;
- semântica;
- procedural;
- memória de si;
- memória de feedback.

### 7. Cognição

- atenção e saliência;
- workspace global;
- detecção de padrões;
- previsão;
- curiosidade;
- metacognição;
- modelo de si;
- motor de objetivos.

### 8. Modelos

- OCR local;
- embeddings;
- modelo multimodal local;
- modelos incrementais leves.

### 9. Orquestração

Agenda módulos, aplica políticas, controla recursos e decide quando invocar modelos pesados.

### 10. Interação

- diálogo;
- notificações;
- avatar;
- explicações;
- pedidos de confirmação.

### 11. Ação

Inicialmente ausente. Posteriormente: preparação, simulação, confirmação, execução e auditoria.

## Fluxo principal

Sinal → Evento bruto → Evento canônico → Timeline → Episódio → Memória → Atenção → Workspace → Hipótese → Metacognição → Pergunta ou silêncio → Feedback → Atualização.

## Independência de capacidades

As listas de sensores, modelos e ações representam possibilidades, não dependências obrigatórias.

Entre os periféricos e a ingestão existe uma camada obrigatória:

```text
Plugins → Capability Registry → Adapters → Canonical Events
```

O núcleo cognitivo consulta operações disponíveis e estados de observabilidade. Ele não conhece implementações concretas. Consulte `PLUGGABLE_CAPABILITY_ARCHITECTURE.md`.

## Implementação em dois ambientes

A arquitetura lógica é implementada em dois contextos:

- referência e experimentação em Python;
- execução implantada em C++.

Essa separação não altera os contratos cognitivos. Consulte `LAB_AND_DEPLOYED_BRAIN_ARCHITECTURE.md`.
