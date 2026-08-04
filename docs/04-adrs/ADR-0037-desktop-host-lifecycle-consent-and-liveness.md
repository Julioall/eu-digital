# ADR-0037 — Lifecycle, consentimento e liveness do host desktop

Status: accepted
Date: 2026-08-04
Accepted: 2026-08-04
Decision authority: delegação explícita do responsável humano para o agente
tomar as decisões necessárias do projeto

## Contexto

A SPEC-050 integra o runtime C++ e o shell Qt, mas seu rascunho e a
implementação preliminar contradizem contratos aceitos:

- um booleano `consent_granted` em `QSettings` substitui indevidamente o ledger
  por sensor/finalidade e a proteção DPAPI da ADR-0026;
- uma superfície interativa “bloqueante” conflita com a apresentação não
  bloqueante da ADR-0016;
- ThreadSanitizer não está disponível na toolchain Windows/Qt selecionada e,
  sozinho, não prova ausência de deadlock;
- o processo cria um manifesto placeholder e usa timestamp fixo em produção;
- o teste existente mede apenas o retorno de `start()`, não tray p99, idle,
  recovery ou ausência de captura.

## Decisão

1. O desktop é um host opcional de composição. Qt, widgets, sensores concretos
   e storage de preferências não entram no núcleo cognitivo.
2. O estado do host usa `DesktopSessionState` 1.0. Estados válidos são
   `onboarding`, `starting`, `running`, `paused`, `degraded`, `stopping` e
   `stopped`; transições e motivo são observáveis sem conteúdo sensorial.
3. Consentimento usa exclusivamente `ConsentLedger`, persistido com DPAPI no
   escopo do usuário. Cada concessão/revogação registra `sensor_id`, `purpose`,
   versão e instante. Ausência, corrupção ou indisponibilidade de DPAPI nega
   captura e mantém o shell em modo degradado/onboarding.
4. Uma ação de onboarding pode conceder vários pares informados de uma vez,
   mas grava um registro independente por par. Sensores concretos só são
   construídos/iniciados quando o par correspondente está permitido.
5. A pausa global é persistida no ledger e tem precedência. Revogação ou pausa
   interrompe a captura antes de publicar novo `CanonicalEvent`.
6. O modal de primeiro uso roda apenas na thread GUI e antes da construção dos
   sensores. A thread cognitiva, presentation port e event bus nunca esperam
   por uma janela. Diálogo normal continua assíncrono pela ADR-0035.
7. Manifesto vem do pacote instalado e dados mutáveis ficam no diretório local
   do usuário. O executável não cria manifesto de runtime, não usa timestamps
   fixos e não grava timeline no diretório de instalação.
8. Um marker local de sessão é criado atomicamente ao iniciar e removido apenas
   após shutdown limpo. Marker anterior leva a `degraded`, executa recovery já
   definido pela ADR-0034 e informa o usuário sem incluir dados sensoriais.
9. Liveness é provado por stress start/pause/resume/stop com watchdog, shutdown
   limitado e execução do caminho Qt offscreen. TSan permanece gate adicional
   em toolchain compatível; sua indisponibilidade no Windows não pode ser
   convertida em alegação de prova.
10. Performance usa `DesktopPerformanceSample` 1.0: tray p99 < 50 ms, frame
    p95 < 16,6 ms, frame p99 < 33 ms, idle CPU < 1% e shutdown limitado. Cada
    medição registra amostras, percentil e ambiente; falha impede conclusão.
11. Ausência de modelo é degradação opcional. O desktop não baixa modelo e não
    chama Ollama diretamente; linguagem só pode entrar pelo renderer removível
    e pelo pipeline estruturado das ADRs 0035/0036.

## Hipótese operacional

`H-SPEC050-DESKTOP`: separar GUI, consentimento e lifecycle do runtime mantém a
observação deny-by-default e a interface responsiva durante carga cognitiva.

- Baseline: `qsettings_boolean_placeholder_desktop_v0`.
- Métricas: capturas antes de consentimento, tray/frame percentis, idle CPU,
  tempo de shutdown, stalls de watchdog e recoveries após marker órfão.
- Ablação: host headless sem Qt e host Qt sem sensores/modelo opcionais.
- Teste metamórfico: remover modelo ou revogar um único sensor altera somente
  capacidade/estado correspondente, sem alterar as demais concessões.
- Falsificação: qualquer captura antes de grant, DPAPI com fallback em claro,
  thread cognitiva aguardando GUI, p99 acima dos limites, shutdown sem remover
  marker ou ausência de modelo impedindo o runtime de operar.

São métricas operacionais de segurança e responsividade, não evidência de
aprendizado ou cognição.

## Consequências

- `QSettings` continua permitido para preferências visuais não sensíveis, nunca
  como fonte de autoridade de consentimento.
- Testes desktop precisam de diretórios e settings isolados para não alterar o
  perfil real do usuário.
- O produto pode iniciar sem sensores/modelo, com diagnóstico explícito.

## Reversão

Desabilitar o target desktop e executar somente o host nativo headless. Ledger,
timeline e snapshots locais permanecem válidos; nenhuma migração do núcleo é
necessária.
