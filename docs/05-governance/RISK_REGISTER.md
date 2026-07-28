# Registro de Riscos

| ID | Risco | Impacto | Mitigação |
|---|---|---|---|
| R-001 | Volume excessivo de eventos | alto | amostragem, compressão e retenção |
| R-002 | Modelo pesado bloqueia sistema | alto | fila, timeout e um modelo por vez |
| R-003 | Padrões espúrios | alto | suporte mínimo, confirmação e calibração |
| R-004 | Perguntas excessivas | médio | orçamento de interrupção |
| R-005 | Deriva de identidade | médio | versionamento do modelo de si |
| R-006 | Contradições acumuladas | alto | reconciliador e estado explícito |
| R-007 | Agente implementador amplia escopo | alto | SPEC, escopo negativo e CI |
| R-008 | OCR inconsistente | médio | árvore de acessibilidade + visão sob demanda |
| R-009 | Armazenamento cresce indefinidamente | alto | políticas de retenção e sumarização |
| R-010 | Inferência emocional indevida | alto | tratar sinais como hipóteses fracas |

| R-011 | Acoplamento oculto a sensor | alto | teste estático de imports e perfis sem sensor |
| R-012 | Fallback altera semântica silenciosamente | alto | auditoria e ajuste de confiança |
| R-013 | Plugin removido quebra memória | alto | eventos canônicos e proveniência persistente |
| R-014 | Nova modalidade contamina crenças | alto | calibração e promoção gradual |
| R-015 | Explosão combinatória de plugins | médio | matriz de compatibilidade e contratos |

| R-016 | Divergência entre referência Python e C++ | alto | contratos únicos, fixtures e equivalência em CI |
| R-017 | Portar algoritmo ainda instável | alto | gate científico anterior à promoção |
| R-018 | Python entrar acidentalmente no instalador | médio | teste de pacote e dependências |
| R-019 | Duplicação manual de schemas | alto | geração a partir de `contracts/` |
| R-020 | Otimização mudar semântica cognitiva | alto | invariantes e comparação longitudinal |

| R-021 | Python e C++ reproduzem o mesmo erro | alto | ground truth e testes metamórficos |
| R-022 | Vazamento do holdout | alto | hashes, acesso registrado e nova rodada após consulta |
| R-023 | Quantização altera calibração | alto | auditoria antes/depois da exportação |
| R-024 | Replay não transfere para sensores reais | alto | gate online e longitudinal |
| R-025 | Timing C++ muda comportamento cognitivo | alto | relógio virtual, tracing e testes de concorrência |
| R-026 | Métrica operacional é confundida com cognição | alto | relatórios e gates separados |
