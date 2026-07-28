# Política de Decisão Científica

Status: normativa

## 1. Classes de fonte

### Classe A

Artigo peer-reviewed com experimento, benchmark reproduzível ou revisão sistemática consolidada.

### Classe B

Revisão peer-reviewed, teoria influente com previsões ou arquitetura implementada parcialmente.

### Classe C

Preprint, relatório técnico, benchmark recente, projeto oficial ou resultado ainda pouco replicado.

### Classe D

Post, demonstração, projeto independente, marketing, analogia ou especulação.

## 2. Requisito para mecanismo cognitivo

Toda nova função cognitiva deve documentar:

- problema;
- mecanismo proposto;
- fontes;
- classe de evidência;
- hipótese;
- baseline;
- métrica;
- ablação;
- critério de falsificação;
- custo;
- riscos;
- alegação máxima permitida.

## 3. Regra de implementação

- Evidência A/B: pode integrar roadmap, sujeita a teste local.
- Evidência C: experimento atrás de feature flag.
- Evidência D: somente sandbox, sem alegação arquitetural.

## 4. Regra de alegações

### Permitido

- “o módulo melhorou a calibração”;
- “o agente manteve memória autobiográfica operacional”;
- “o self-model alterou decisões por ablação”;
- “o sistema aprendeu padrões sem rótulos iniciais”.

### Proibido sem evidência extraordinária

- “o agente sente”;
- “o agente é consciente”;
- “o agente deseja”;
- “o avatar expressa emoções reais”;
- “o sistema compreendeu como um humano”.

## 5. Revisão de arquitetura

Toda ADR cognitiva deve citar a matriz de evidência e indicar como será testada.

## 6. Resultados negativos

Resultados negativos não podem ser omitidos. Devem atualizar:

- matriz de evidência;
- roadmap;
- risco;
- status da hipótese;
- decisão de manter, alterar ou remover o módulo.

## 7. Reprodutibilidade

Experimentos devem registrar:

- versão do código;
- configuração;
- modelos;
- quantização;
- hardware;
- dataset;
- seeds;
- duração;
- métricas;
- logs;
- limitações.

## Promoção para o runtime

Uma decisão científica aprovada ainda não autoriza implantação. O componente deve passar pelo pipeline Python→C++ definido na ADR-0010 e na SPEC-026.

A equivalência deve ser avaliada antes da otimização final. Alterações semânticas produzidas durante a porta exigem nova avaliação científica.

## 8. Verificação, validade e transferência

Toda decisão deve distinguir:

- verificação de implementação;
- generalização computacional;
- validade científica;
- validade ecológica;
- efeito causal.

A referência Python não constitui verdade científica. Resultados devem usar ground truth, invariantes, holdout e ablação quando aplicáveis.

## 9. Holdout e preregistro

Métricas, limiares e critérios de exclusão devem ser congelados antes da avaliação final. Consultar o holdout e alterar o mecanismo inicia nova rodada experimental.
