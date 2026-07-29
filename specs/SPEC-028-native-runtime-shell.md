---
id: SPEC-028
title: Runtime local mínimo do Cérebro Implantado
status: ready
phase: 0.3
dependencies: [SPEC-002, SPEC-006, SPEC-023, SPEC-025, SPEC-026, SPEC-027]
adrs: [ADR-0010, ADR-0011, ADR-0024]
contracts: [runtime_manifest.schema.json, runtime_health.schema.json]
---

# SPEC-028 — Runtime local mínimo do Cérebro Implantado

Status: ready
Fase: 0.3
Dependências: SPEC-002, SPEC-006, SPEC-023, SPEC-025, SPEC-026, SPEC-027
ADRs aplicáveis: ADR-0010, ADR-0011, ADR-0024

## Objetivo

Transformar o executável mínimo de verificação em um host C++ local,
controlado explicitamente, que inicializa o núcleo operacional, publica seu
estado de saúde, usa a timeline local e encerra de forma previsível sem
depender de Python, rede, modelo ou sensor obrigatório.

Esta é uma SPEC de infraestrutura operacional. Ela não afirma consciência,
experiência ou validade cognitiva.

## Entregáveis

- `RuntimeHost` C++ com ciclo de vida explícito e idempotente;
- modo controlado de verificação e replay determinístico;
- carregamento de um manifesto e emissão de `RuntimeHealth`;
- integração do host com event bus, capability registry e timeline;
- tratamento estruturado de falhas de inicialização, degradação e encerramento;
- pacote CMake/CPack contendo apenas o runtime nativo e seus artefatos aprovados;
- testes nativos e de instalação para Windows e Linux/WSL quando o toolchain
  estiver disponível;
- documentação operacional reproduzível.

## Requisitos

1. O host deve expor os estados `starting`, `ready`, `degraded`, `stopping`,
   `stopped` e `failed`.
2. `start` e `stop` devem ser seguros quando chamados repetidamente.
3. O host deve iniciar sem sensores de domínio ou atuadores instalados.
4. A ausência de uma capacidade opcional deve ser registrada explicitamente e
   não pode gerar observação negativa.
5. Falhas não fatais devem produzir `degraded` e erro estruturado; falhas
   fatais devem produzir `failed` e encerrar com código não zero.
6. Eventos aceitos pelo host devem preservar o contrato `CanonicalEvent` e
   ser persistidos pela timeline configurada.
7. Um replay da mesma fixture, relógio e configuração deve produzir o mesmo
   snapshot operacional, salvo campos de tempo explicitamente marcados.
8. O manifesto deve identificar build, versões de contrato, commit e
   componentes promovidos.
9. O snapshot de saúde deve validar contra `RuntimeHealth`.
10. O pacote instalado não pode conter `.py`, ambiente virtual, notebooks,
    datasets de pesquisa ou pesos não aprovados.
11. O runtime não pode abrir conexões de rede, baixar componentes ou iniciar
    automaticamente com o sistema.
12. Logs não podem conter conteúdo sensorial bruto, credenciais ou texto de
    clipboard.

## Escopo negativo

- serviço Windows, daemon ou auto-start;
- interface gráfica, avatar ou escolha de toolkit;
- seleção, download ou empacotamento de modelo local;
- captura real de tela, teclado, mouse, áudio ou câmera pelo host;
- execução de ações ou atuadores concretos;
- promoção automática de componentes Python;
- telemetria externa ou qualquer dependência de nuvem;
- reimplementação de mecanismos cognitivos ainda não promovidos;
- alegações de consciência, emoção ou intenção fenomenal.

## Critérios de aceite

- [ ] `RuntimeHost` inicia em processo nativo sem interpretador Python.
- [ ] Inicialização sem capacidades opcionais termina em `ready`.
- [ ] Falha de uma dependência obrigatória termina em `failed` com erro
      estruturado e código não zero.
- [ ] Indisponibilidade de capacidade opcional produz `degraded` sem inventar
      observação.
- [ ] `start` e `stop` repetidos não duplicam sessão, evento ou fechamento.
- [ ] Manifesto válido é carregado e manifesto inválido é rejeitado.
- [ ] `RuntimeHealth` é serializado e validado contra seu schema.
- [ ] Um evento canônico aceito é persistido e recuperado pela timeline.
- [ ] Replay determinístico passa com fixture congelada e relógio controlado.
- [ ] Reinício recupera o estado permitido sem apagar o histórico.
- [ ] CTest cobre ciclo de vida, falha, replay, timeline e contratos.
- [ ] Instalação CMake/CPack contém o binário nativo e não contém arquivos
      Python.
- [ ] O smoke test funciona no Windows nativo e no ambiente Linux/WSL
      suportado pelo projeto.
- [ ] Documentação descreve instalação, operação, diagnóstico e remoção
      reversível.

## Protocolo operacional e científico

Hipótese operacional: um host nativo explícito reduz falhas silenciosas de
inicialização e torna a degradação observável sem aumentar as dependências do
núcleo.

- baseline: executável atual de validação de fixture e execução sem host;
- métricas: tempo de inicialização, código de saída, transições válidas,
  eventos persistidos/recuperados, erros classificados e conteúdo do pacote;
- ablação: host sem health snapshot e host sem recuperação de timeline, usando
  a mesma sequência de eventos;
- falsificação: o host perde eventos, inventa observações por ausência,
  recupera estado incompatível, depende de Python/rede ou não distingue falha
  fatal de degradação opcional.

Estas métricas são operacionais. Não constituem evidência de validade
cognitiva.

## Testes obrigatórios

- testes unitários C++ do ciclo de vida e das transições inválidas;
- testes de contrato para os dois schemas e fixtures válidas/inválidas;
- integração com event bus e timeline em diretório temporário;
- replay e reinício com relógio controlado;
- injeção de falha de manifesto, timeline e capacidade obrigatória;
- teste de empacotamento que procure arquivos Python e conexões externas;
- smoke test nativo em cada plataforma suportada;
- execução de lint, tipos, testes Python e validação híbrida sem alterar
  contratos anteriores.

## Dependências e bloqueios

Esta SPEC não depende de decidir o modelo multimodal, o toolkit do avatar, a
retenção de áudio ou a política de auto-start. A compilação Windows exige um
toolchain C++ e SQLite locais; essa é uma dependência operacional do ambiente,
não uma dependência estrutural do núcleo.
