# Arquitetura do Laboratório Python e do Cérebro Implantado C++

Status: normativa  
Versão: 1.0  
Decisão associada: ADR-0010

## 1. Decisão

O projeto será desenvolvido como um único programa científico e de engenharia, organizado em dois ambientes complementares:

1. **Laboratório Python** — exploração, treinamento, experimentação, implementações de referência e validação científica.
2. **Cérebro Implantado C++** — runtime local instalável, permanente, otimizado e independente de Python.

Eles compartilham contratos, datasets, modelos exportados, métricas e testes de equivalência.

Não são dois produtos concorrentes. O Laboratório produz conhecimento e artefatos para o Cérebro Implantado.

## 2. Princípio central

```text
Python descobre e valida.
C++ executa e sustenta.
```

Uma hipótese cognitiva não entra no runtime final apenas porque funciona em um notebook. Ela precisa demonstrar valor mensurável, possuir contrato estável e passar por implementação e validação equivalentes em C++.

## 3. Monorepositório

Estrutura normativa:

```text
eu-digital/
├── cpp/
│   ├── core/
│   ├── runtime/
│   ├── app/
│   ├── plugins/
│   ├── inference/
│   ├── storage/
│   ├── tests/
│   └── benchmarks/
│
├── python/
│   ├── eu_digital_lab/
│   ├── reference/
│   ├── prototypes/
│   ├── training/
│   ├── analysis/
│   ├── notebooks/
│   └── tests/
│
├── contracts/
│   ├── schemas/
│   ├── protocol/
│   ├── fixtures/
│   └── compatibility/
│
├── datasets/
│   ├── raw/
│   ├── normalized/
│   ├── annotated/
│   └── synthetic/
│
├── models/
│   ├── source/
│   ├── exported/
│   ├── manifests/
│   └── benchmarks/
│
├── validation/
│   ├── equivalence/
│   ├── ablations/
│   ├── longitudinal/
│   └── reports/
│
├── experiments/
├── docs/
├── specs/
└── tools/
```

## 4. Laboratório Python

### Responsabilidades

- prototipar mecanismos cognitivos;
- criar implementações de referência;
- treinar e ajustar modelos;
- preparar e anotar datasets;
- executar experimentos e ablações;
- calcular métricas;
- analisar logs;
- gerar gráficos e relatórios;
- converter modelos;
- definir fixtures de comportamento esperado;
- testar novas modalidades antes da implementação nativa.

### Pode conter

- PyTorch;
- NumPy;
- SciPy;
- pandas;
- scikit-learn;
- Jupyter;
- bibliotecas experimentais;
- código deliberadamente descartável;
- implementações lentas, desde que corretas e mensuráveis.

### Não é obrigado a conter

- instalador;
- avatar final;
- sensores Windows de produção;
- execução contínua por meses;
- otimização extrema;
- compatibilidade binária;
- comportamento em tempo real.

### Regra de referência

Quando um algoritmo Python for declarado como implementação de referência, sua versão, parâmetros, sementes, dataset e tolerâncias tornam-se imutáveis para a rodada de promoção correspondente.

## 5. Cérebro Implantado C++

### Responsabilidades

- runtime permanente;
- ciclo cognitivo;
- capability registry;
- event bus;
- timeline;
- memória operacional e persistente;
- self-model;
- scheduler;
- supervisão de recursos;
- sensores nativos;
- ferramentas;
- atuadores;
- inferência por runtimes nativos;
- interface e avatar;
- atualização e instalação;
- recuperação após falhas.

### Restrições

- não depender de interpretador Python;
- não executar notebooks;
- não importar módulos Python;
- não instalar pacotes por `pip`;
- não requerer ambiente Conda;
- não depender de serviços em nuvem;
- não aceitar um algoritmo sem teste de equivalência e benchmark;
- não incorporar código de pesquisa não estabilizado no caminho crítico.

### Tecnologias-alvo

- C++23;
- CMake;
- Ninja ou Visual Studio;
- Qt 6/QML para interface;
- Win32, COM, DXGI, WASAPI, Raw Input e UI Automation;
- ONNX Runtime, OpenVINO, llama.cpp ou backend nativo selecionado;
- formatos binários e memória compartilhada quando aplicáveis.

## 6. Fronteira entre os ambientes

A fronteira é composta por artefatos, não por chamadas Python em produção.

```text
Laboratório Python
   │
   ├── contratos versionados
   ├── datasets e fixtures
   ├── parâmetros validados
   ├── modelos exportados
   ├── resultados esperados
   └── relatórios científicos
             ↓
Cérebro Implantado C++
```

Artefatos permitidos:

- ONNX;
- GGUF;
- OpenVINO IR;
- TensorRT engine quando o hardware permitir;
- Protocol Buffers;
- FlatBuffers;
- JSON Schema para configuração;
- arquivos de parâmetros;
- datasets de replay;
- manifestos de modelos;
- relatórios de equivalência.

## 7. Fluxo de promoção

```text
hipótese
→ protótipo Python
→ baseline
→ experimento
→ ablação
→ decisão científica
→ contrato congelado
→ implementação C++
→ equivalência funcional
→ benchmark
→ teste longitudinal
→ aprovação
→ runtime instalado
```

Cada promoção recebe um `promotion_id`.

## 8. Critérios mínimos de promoção

Um componente só pode sair do Laboratório quando:

- a hipótese está documentada;
- existe baseline;
- métricas e limiares foram definidos antes do teste final;
- o resultado foi reproduzido;
- existe dataset ou replay versionado;
- a implementação Python de referência está congelada;
- os inputs e outputs possuem schema;
- estados e erros possuem semântica explícita;
- a tolerância de equivalência foi definida;
- o custo esperado foi medido;
- o comportamento sem sensores opcionais foi avaliado;
- a remoção do componente foi testada por ablação.

## 9. Tipos de equivalência

### Exata

Usada para:

- parsing;
- schemas;
- transições de estado;
- ordenação;
- hashes;
- regras determinísticas.

### Numérica

Usada para:

- scores;
- probabilidades;
- embeddings;
- filtros;
- modelos incrementais.

Deve declarar tolerância absoluta e relativa.

### Estatística

Usada quando aleatoriedade ou hardware altera resultados individuais.

Deve comparar:

- distribuição;
- média e variância;
- intervalo de confiança;
- calibração;
- desempenho longitudinal.

### Comportamental

Usada para mecanismos com múltiplas saídas aceitáveis.

Deve comparar invariantes, restrições e métricas de tarefa, não igualdade textual.

## 10. Modelos de IA

Treinamento e experimentação podem ocorrer em Python.

A execução final usa runtime nativo:

```text
PyTorch / Transformers no laboratório
              ↓ exportação
ONNX / GGUF / OpenVINO IR / engine otimizada
              ↓
runtime C++
```

O modelo exportado precisa possuir manifesto contendo:

- origem;
- licença;
- hash;
- versão;
- dataset de avaliação;
- configuração de exportação;
- quantização;
- limites conhecidos;
- hardware validado;
- métricas antes e depois da exportação.

## 11. Sensores e plugins

Sensores de produção são preferencialmente C++ quando dependem de APIs nativas ou alto volume.

O Laboratório pode usar:

- sensores simulados;
- replays;
- wrappers Python;
- mocks;
- versões de baixa frequência.

Ambos devem produzir o mesmo evento canônico. O algoritmo cognitivo não pode depender da linguagem do sensor.

## 12. Divergência proibida

Python e C++ não podem possuir definições independentes para:

- eventos;
- episódios;
- hipóteses;
- capacidades;
- estados de sensores;
- ações;
- self-model;
- formatos de memória;
- métricas;
- IDs;
- relógios;
- versionamento.

Essas definições pertencem a `contracts/`.

## 13. Versões experimentais

O Laboratório pode manter várias variantes:

```text
episode_segmenter/
├── v1_threshold/
├── v2_bayesian/
└── v3_learned/
```

O Cérebro Implantado contém somente versões aprovadas:

```text
cpp/plugins/episode_segmenter_v3/
```

Variantes descartadas continuam documentadas como resultados negativos.

## 14. Release

O instalador final inclui:

- executáveis C++;
- DLLs e plugins nativos aprovados;
- modelos exportados;
- configurações;
- schemas necessários para validação;
- assets da interface;
- licenças;
- ferramentas de diagnóstico.

Não inclui:

- código-fonte Python;
- ambiente virtual;
- notebooks;
- datasets de pesquisa;
- pesos de treinamento não necessários;
- dependências de desenvolvimento.

## 15. Regra de desempenho

A implementação C++ deve demonstrar pelo menos uma destas propriedades em relação à referência:

- menor latência;
- menor consumo;
- maior previsibilidade;
- maior estabilidade;
- integração nativa;
- empacotamento independente;
- execução contínua confiável.

Uma porta C++ que só duplica complexidade sem benefício operacional pode ser rejeitada ou adiada.

## 16. Identidade científica do projeto

O Cérebro Implantado não é uma reescrita desconectada do Laboratório. Cada mecanismo implantado precisa ser rastreável até:

- hipótese;
- referência científica;
- experimento;
- implementação de referência;
- promoção;
- commit;
- benchmark;
- versão instalada.

## 17. Limite científico da implementação de referência

A implementação Python é uma referência de comportamento e regressão. Ela não constitui ground truth.

Uma promoção deve validar separadamente:

- Python contra contrato e ground truth;
- C++ contra contrato e ground truth;
- Python contra C++;
- versões exportadas e quantizadas;
- comportamento em replay e online.

Consulte ADR-0011 e SPEC-027.
