# Arquitetura de Capacidades Removíveis

Status: normativa  
Princípio: núcleo cognitivo independente de modalidade

## 1. Objetivo

Permitir que sensores, ferramentas, atuadores e modelos sejam adicionados, substituídos, desativados ou removidos sem modificar o núcleo cognitivo e sem destruir sua continuidade operacional.

A analogia funcional é a seguinte:

- o núcleo mantém identidade, memória, atenção, previsão e metacognição;
- os periféricos oferecem canais de percepção e ação;
- a perda de um canal reduz capacidades e aumenta incerteza;
- a chegada de um novo canal inicia descoberta, calibração e aprendizagem;
- nenhum canal isolado define o agente.

A arquitetura não afirma equivalência biológica com neuroplasticidade humana. Ela implementa adaptação funcional mensurável.

## 2. Regra de dependência

O núcleo cognitivo pode depender apenas de:

- `CanonicalEvent`;
- `CapabilityDescriptor`;
- `CapabilityState`;
- `ObservationEnvelope`;
- `ActionRequest`;
- `ActionOutcome`;
- serviços abstratos de memória e tempo.

O núcleo não pode importar bibliotecas ou classes concretas de:

- captura de tela;
- OCR;
- microfone;
- câmera;
- teclado;
- mouse;
- navegador;
- sistema de arquivos;
- modelos específicos;
- automação de interface.

A dependência deve apontar para dentro:

```text
Plugin concreto
    ↓ implementa
Porta/contrato abstrato
    ↓ publica
Evento ou capacidade canônica
    ↓ consumido por
Núcleo cognitivo
```

## 3. Tipos de capacidade

### Sensor

Produz observações sobre ambiente ou estado interno.

Exemplos: visão da tela, OCR, áudio, câmera, eventos de processos e telemetria interna.

### Ferramenta

Executa operação consultiva ou transformativa sem representar ação contínua.

Exemplos: busca local, parser, calculadora, indexador e transcritor.

### Atuador

Produz alteração no ambiente.

Exemplos: mover avatar, clicar, digitar, criar arquivo ou controlar aplicação.

### Modelo

Realiza inferência especializada.

Exemplos: embeddings, visão-linguagem, reconhecimento de fala e previsão temporal.

### Serviço cognitivo

Oferece memória, saliência, consolidação ou avaliação. Serviços cognitivos também devem ser substituíveis, mas obedecem contratos mais rígidos de migração de estado.

## 4. Registro de capacidades

O `CapabilityRegistry` é a única fonte operacional sobre o que o agente pode usar agora.

Cada entrada informa:

- identificador estável;
- versão;
- tipo;
- operações fornecidas;
- modalidades;
- schemas aceitos e emitidos;
- dependências obrigatórias;
- dependências opcionais;
- estado;
- qualidade;
- latência;
- custo de recursos;
- permissões;
- política de inicialização;
- política de desligamento;
- compatibilidade;
- health check.

O registro emite eventos quando uma capacidade:

- é descoberta;
- entra em calibração;
- fica disponível;
- degrada;
- falha;
- é desativada;
- é removida;
- é atualizada.

## 5. Descoberta e hot-plug

Módulos podem ser descobertos por:

- diretório local de plugins;
- entry points do ambiente Python;
- manifesto configurado;
- processo local supervisionado;
- adaptador IPC futuro.

O hot-plug deve seguir:

```text
descoberta
→ validação de manifesto
→ verificação de contrato
→ verificação de permissões
→ health check
→ calibração
→ registro
→ evento de disponibilidade
→ atualização do modelo de si
```

A remoção deve seguir:

```text
pedido ou falha
→ impedir novas requisições
→ concluir ou cancelar operações
→ produzir estado final
→ persistir checkpoint quando aplicável
→ retirar do registro
→ invalidar planos dependentes
→ atualizar modelo de si
→ recalcular atenção, confiança e objetivos
```

## 6. Ausência como estado explícito

Ausência não é erro excepcional. É estado operacional.

Estados mínimos:

- `unknown`;
- `discovered`;
- `calibrating`;
- `available`;
- `degraded`;
- `temporarily_unavailable`;
- `disabled`;
- `failed`;
- `removed`;
- `incompatible`.

Quando uma capacidade faltar, o agente deve:

1. reconhecer a ausência;
2. atualizar o modelo de si;
3. identificar crenças e planos afetados;
4. reduzir confiança quando apropriado;
5. procurar capacidades equivalentes;
6. adaptar a estratégia;
7. explicar a limitação;
8. continuar operando com o subconjunto disponível.

## 7. Substituição e equivalência

Capacidades são escolhidas por operação e qualidade, não por nome de implementação.

Exemplo:

```text
operação necessária: extract_visible_text

candidatos:
- accessibility_tree_reader
- pp_ocr
- multimodal_model
```

A seleção considera:

- compatibilidade;
- confiança esperada;
- latência;
- custo;
- contexto;
- disponibilidade;
- histórico de desempenho.

Fallbacks devem ser declarados e auditáveis. O sistema nunca deve trocar de fonte silenciosamente quando a mudança altera a confiança ou semântica da observação.

## 8. Entrada de novas modalidades

Uma modalidade desconhecida pode ser incorporada se o plugin fornecer:

- schema de observação;
- relógio e alinhamento temporal;
- descrição semântica;
- unidade e escala;
- limites conhecidos;
- qualidade estimada;
- procedimento de calibração;
- exemplos de eventos;
- política de retenção.

O núcleo não precisa compreender imediatamente toda a modalidade. Inicialmente pode:

- armazenar eventos;
- calcular novidade;
- correlacionar com outras fontes;
- solicitar rótulos;
- aprender representações;
- promover características úteis após validação.

## 9. Plasticidade funcional

A adaptação à perda ou adição de sensores ocorre em quatro níveis:

### Atenção

Redistribui orçamento entre fontes disponíveis.

### World model

Recalibra previsões para observabilidade parcial.

### Metacognição

Aumenta incerteza e identifica o que não pode mais ser conhecido.

### Planejamento

Evita ações sem atuadores e escolhe rotas alternativas.

Memórias antigas preservam a modalidade e a proveniência originais, mesmo após remoção do plugin.

## 10. Degradação graciosa

O sistema deve possuir perfis testados:

- sem visão;
- sem áudio;
- sem eventos de teclado;
- sem LLM;
- sem embeddings;
- sem atuadores;
- apenas timeline;
- múltiplos sensores simultaneamente indisponíveis.

A degradação é aceitável quando:

- o processo permanece íntegro;
- nenhuma observação é inventada;
- limitações são corretamente relatadas;
- planos incompatíveis são bloqueados;
- memórias continuam legíveis;
- o retorno da capacidade restaura funcionamento sem recriar o agente.

## 11. Separação entre identidade e capacidade

O agente continua sendo a mesma instância histórica após perder ou ganhar um sensor. O self-model deve distinguir:

- identidade persistente;
- capacidades atuais;
- capacidades históricas;
- capacidades potenciais;
- capacidades degradadas;
- experiências adquiridas por cada modalidade.

Uma mudança de capacidade cria uma nova versão do self-model, não uma nova identidade.

## 12. Critérios arquiteturais

Um módulo é realmente removível apenas quando:

- o núcleo inicia sem ele;
- os testes passam no perfil de ausência;
- nenhum import concreto existe no núcleo;
- o registro representa sua ausência;
- operações dependentes são bloqueadas ou redirecionadas;
- reinstalação não exige migração manual do núcleo;
- eventos antigos continuam válidos;
- o módulo pode ser substituído por implementação compatível.
