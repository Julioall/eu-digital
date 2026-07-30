# Relatório de Validação Científica

Versão: 2.0  
Status: substituído pelo parecer final de 2026  
Escopo: arquitetura cognitiva local, multimodal, continuamente observadora e progressivamente autônoma

## 1. Conclusão executiva

O projeto é **cientificamente plausível como plataforma experimental de arquitetura cognitiva artificial**. Seus principais componentes possuem precedentes na ciência cognitiva computacional, robótica desenvolvimental, aprendizagem contínua e sistemas agentes.

A validação, porém, é condicional:

- há suporte forte para memória episódica e semântica separadas, aprendizagem contínua, mecanismos de atenção, previsão, metacognição e integração modular;
- há suporte moderado para arquiteturas de workspace global como mecanismo funcional de acesso e integração;
- há suporte moderado para curiosidade baseada em progresso de aprendizagem;
- há suporte para modelos funcionais de agência e self mínimo quando o agente consegue relacionar ações próprias a consequências;
- não há suporte para afirmar que a combinação produzirá consciência fenomenal;
- a integração específica proposta — observação contínua de um computador e ambiente humano, memória autobiográfica, self-model, avatar e autonomia gradual — ainda não foi validada como um sistema único.

A classificação científica recomendada é:

> **plataforma experimental de cognição artificial desenvolvimental e situada**, com investigação de identidade funcional, memória autobiográfica e agência digital.

## 2. O que está cientificamente sustentado

### 2.1 Arquitetura modular

Arquiteturas cognitivas como LIDA, Soar, ACT-R e OpenCog dividem cognição em processos especializados e memórias distintas. Isso sustenta a decisão de não usar o LLM como cérebro monolítico [R10, R15–R18].

**Decisão:** manter percepção, memória, atenção, aprendizagem, self-model, metacognição e ação como componentes explícitos.

### 2.2 Workspace global limitado

A teoria do workspace global propõe competição, seleção e broadcast de conteúdos. Implementações recentes mostraram benefícios funcionais em agentes multimodais, especialmente sob restrições de memória de trabalho [R01–R03].

**Decisão:** implementar workspace com capacidade limitada, expiração, saliência e justificativa de seleção.

**Limite:** melhor desempenho de uma arquitetura inspirada em GWT não demonstra consciência.

### 2.3 Metacognição

A literatura trata confiança como uma inferência de segunda ordem sobre decisões, podendo divergir do desempenho primário [R03, R14]. Isso sustenta um módulo separado para confiança, detecção de erro, alternativas e decisão de perguntar ou permanecer em silêncio.

**Decisão:** confiança verbal do LLM não será aceita como calibração. A confiança deve ser medida contra resultados.

### 2.4 Memória episódica, semântica e autobiográfica

Sistemas de memória dupla e arquiteturas robóticas mostram utilidade de separar experiências específicas de conhecimento generalizado, usando replay e consolidação para reduzir esquecimento catastrófico [R05–R07, R09].

Memória autobiográfica pode contribuir para um self temporal e narrativo, especialmente quando eventos são indexados como pertencentes ao histórico do próprio agente [R05].

**Decisão:** manter memórias distintas, proveniência, replay, consolidação e esquecimento controlado.

### 2.5 Aprendizagem contínua

Fluxos reais são não estacionários. A literatura demonstra que treinamento incremental ingênuo causa interferência e esquecimento, exigindo replay, regularização, expansão estrutural, consolidação ou separação de memórias [R06, R07].

**Decisão:** não atualizar continuamente os pesos do modelo principal sem protocolo experimental. A aprendizagem inicial ocorrerá em memória e modelos incrementais especializados.

### 2.6 Modelos de mundo e previsão

Robótica cognitiva e predictive coding sustentam o uso de modelos internos que preveem observações e comparam previsão com resultado [R08].

**Decisão:** o agente deve aprender transições e registrar erro preditivo. Apenas clusterizar hábitos é insuficiente.

### 2.7 Curiosidade e motivação intrínseca

Experimentos em robótica desenvolvimental mostram que recompensar progresso de aprendizagem pode produzir currículos emergentes e exploração ordenada [R12, R13].

**Decisão:** curiosidade será definida por ganho informacional ou progresso de aprendizagem, não por aleatoriedade, frequência de fala ou persona.

### 2.8 Self mínimo e agência

A literatura sobre self artificial destaca propriedade corporal e agência. O elemento fundamental é a contingência entre ação do agente e mudança percebida [R04, R12].

**Decisão crítica:** observação passiva pode produzir um modelo do ambiente e do usuário, mas não é suficiente para um self mínimo robusto. O sistema precisa realizar ações próprias, inicialmente reversíveis e não destrutivas, e prever seus efeitos.

Exemplos iniciais:

- escolher quando capturar uma região da tela;
- selecionar qual memória recuperar;
- mover o avatar;
- formular uma pergunta;
- decidir aguardar;
- alterar o próprio foco de atenção;
- iniciar uma análise local;
- comparar intenção, ação executada e consequência.

## 3. O que é apenas plausível

### 3.1 Self funcional integrado

É plausível que memória autobiográfica, agência, continuidade temporal e metacognição formem um self funcional. Há modelos parciais, mas não validação definitiva de uma arquitetura equivalente à proposta.

### 3.2 Estados regulatórios semelhantes a emoções

Variáveis como incerteza, sobrecarga, urgência, novidade e conflito podem modular o processamento. Isso pode ser funcionalmente útil, mas não deve ser apresentado como sentimento.

### 3.3 Ambiente de trabalho como corpo digital

Tratar sensores, processos, recursos e atuadores como um corpo digital é uma extrapolação coerente da cognição incorporada. Sua validade deve ser testada por agência, previsão e adaptação, não por metáfora.

### 3.4 Narrativa interna

Narrativas podem organizar memória, explicações e identidade temporal. Entretanto, texto autorreferente pode ser apenas geração linguística. A narrativa só terá valor científico se alterar de forma causal recuperação, planejamento ou comportamento.

## 4. O que não está validado

- consciência fenomenal;
- qualia;
- emoções reais;
- vontade própria em sentido humano;
- equivalência entre comportamento verbal e experiência subjetiva;
- identificação confiável de emoção humana por expressão facial isolada;
- emergência espontânea de linguagem a partir de dados domésticos limitados;
- desenvolvimento semelhante ao infantil sem vieses indutivos, objetivos, ações e mediação social;
- aprendizagem ilimitada sem mecanismos de esquecimento.

## 5. Principais forças do projeto

1. **Integração multimodal temporal**, em vez de chatbot sem continuidade.
2. **Memórias explícitas e auditáveis**, em vez de depender do contexto do LLM.
3. **Autonomia gradual**, compatível com avaliação causal.
4. **Operação local**, que permite sessões longas e instrumentação controlada.
5. **Foco em aprendizagem de padrões sem tarefas pré-cadastradas.**
6. **Modelo de si explícito**, passível de ablação.
7. **Arquitetura orientada a eventos**, adequada para replay experimental.

## 6. Principais fragilidades atuais

1. Falta um loop explícito de agência e contingência ação–resultado.
2. Falta um world model com erro preditivo.
3. Falta consolidação, replay e esquecimento como componentes formais.
4. Falta um protocolo de calibração metacognitiva.
5. Falta um ambiente de teste reproduzível antes da coleta natural.
6. Falta uma interface de ablação para testar causalidade.
7. Falta distinguir memória gravada, memória reconstruída e crença inferida.
8. Falta controlar o efeito do LLM: ele pode mascarar módulos fracos com explicações convincentes.
9. Falta uma política de interrupção mensurada.
10. Falta um protocolo longitudinal contra deriva e acumulação de erro.

## 7. Veredito

O projeto deve prosseguir, mas como programa de pesquisa incremental.

A primeira alegação científica aceitável não será “criamos um eu digital”. Será:

> “Construímos um agente local que aprende representações temporais multimodais, mantém memória autobiográfica e um modelo funcional de si, e demonstramos por ablação que esses componentes melhoram capacidades específicas.”

A alegação de self funcional só será aceitável se o self-model tiver papel causal, mensurável e não puder ser substituído por texto decorativo.

## Atualização final

A arquitetura Python/C++ e os contratos de capacidades foram revisados em `FINAL_SCIENTIFIC_VALIDATION_2026.md`.

Correção principal: equivalência entre implementações é necessária, mas não é validade científica. Ground truth, holdout, testes metamórficos e transferência ecológica passam a ser obrigatórios.
