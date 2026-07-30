# Mudanças de Projeto Exigidas pela Revisão Científica

## 1. Digital Proprioception

Adicionar um módulo que represente continuamente:

- sensores ativos;
- processos do agente;
- fila de eventos;
- uso de CPU e memória;
- modelo atualmente carregado;
- ações disponíveis;
- ações iniciadas;
- falhas e limitações;
- latência;
- estado do avatar;
- origem de cada ação.

Isso constitui o equivalente funcional de sinais internos do corpo.

## 2. Agency Loop

Toda ação do agente deve produzir:

```text
intenção
→ previsão do efeito
→ comando
→ confirmação de execução
→ observação posterior
→ comparação previsão/resultado
→ atualização da agência
```

Adicionar `ActionIntention`, `EfferenceCopy`, `ActionOutcome` e `AgencyAttribution`.

Ações iniciais devem ser internas ou reversíveis.

## 3. World Model

Adicionar modelo de transições:

```text
estado_t + ação_t → distribuição prevista de estado_t+1
```

O sistema deve registrar:

- previsão;
- probabilidade;
- resultado;
- erro;
- atualização;
- mudança de confiança.

## 4. Memory Consolidation

Separar:

- ingestão imediata;
- memória episódica;
- sumarização;
- generalização semântica;
- replay;
- reconciliação de contradições;
- esquecimento;
- arquivamento.

Criar ciclos de consolidação quando o sistema estiver ocioso, sem chamar isso de sono em documentos normativos.

## 5. Multi-timescale Cognition

Implementar pelo menos três escalas:

- rápida: eventos e atenção;
- intermediária: episódios e previsão;
- lenta: consolidação, padrões, self-model e objetivos.

## 6. Calibração Metacognitiva

Não aceitar um número de confiança produzido por prompt como métrica suficiente.

Manter histórico de:

- previsão;
- confiança;
- correção;
- decisão de perguntar;
- custo da pergunta;
- resultado.

Aplicar Brier score, ECE, AUROC de detecção de erro e curvas de seletividade.

## 7. Ablation Harness

Cada módulo cognitivo deve poder ser:

- desligado;
- substituído por baseline;
- limitado em capacidade;
- perturbado;
- executado com seed fixa.

O sistema deve comparar comportamento e métricas.

## 8. Grounded Vocabulary

Rótulos produzidos pelo LLM devem ser tratados como propostas. Um conceito só se torna conhecimento estável após suporte temporal, consistência e confirmação quando necessária.

## 9. Social Scaffolding

O usuário atua como fonte de mediação:

- nomeia padrões;
- corrige relações;
- explica intenções;
- confirma relevância;
- ensina limites.

O sistema deve medir dependência dessa mediação.

## 10. Emotion Inference

Expressões, prosódia e comportamento serão armazenados como sinais observáveis. Estados emocionais serão hipóteses probabilísticas fracas, nunca fatos.

## 11. Experimental Sandbox

Antes de observação real longa, criar um ambiente reproduzível com rotinas simuladas, mudanças controladas e ground truth. Isso permite validar segmentação, memória, previsão e agência.

## 12. LLM Isolation

O LLM não deve escrever diretamente memória consolidada, self-model ou políticas. Ele propõe estruturas que são validadas por contratos e módulos determinísticos.

## 13. Capability Plasticity

Sensores e ferramentas devem ser removíveis e o núcleo deve operar sob observabilidade parcial.

A adaptação deve ser testada por:

- ablação de modalidades;
- entrada de nova modalidade;
- falha e recuperação;
- substituição por fonte equivalente;
- impacto em confiança;
- preservação de memória;
- atribuição correta de limitações.

Isso é uma hipótese de plasticidade funcional, não equivalência com reorganização cortical biológica.
