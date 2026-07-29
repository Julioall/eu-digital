# ADR-0024 — Host nativo e ciclo de vida local do Cérebro Implantado

Status: accepted
Data: 2026-07-29
Decisores: aprovação humana do projeto

## Contexto

As SPECs 025–027 entregaram o monorepositório, a fronteira Python/C++ e os
gates de promoção e validação. O executável C++ atual valida uma fixture, mas
ainda não possui um host operacional com ciclo de vida, estado de saúde e
persistência coordenados.

Uma próxima fase precisa tornar o Cérebro Implantado executável e auditável
sem escolher ainda um modelo, uma interface gráfica ou sensores obrigatórios.

## Opções consideradas

### Serviço Windows com inicialização automática

Adiado. Ele introduziria políticas de instalação, atualização, permissões e
recuperação que ainda não possuem contrato aprovado.

### Aplicação gráfica com avatar

Adiada. A escolha de toolkit permanece aberta e já é tratada como uma questão
de produto da futura fase de interação.

### Host nativo de console, controlado explicitamente

Escolhido para a primeira fase operacional. Ele pode ser executado em modo de
verificação, replay ou operação controlada, sem auto-start e sem captar dados
por conta própria.

## Decisão

O runtime C++ terá um `RuntimeHost` local com:

- estados explícitos de ciclo de vida: `starting`, `ready`, `degraded`,
  `stopping`, `stopped` e `failed`;
- inicialização e encerramento idempotentes;
- integração com o event bus, o capability registry e a timeline local;
- ausência de capacidades opcionais representada como estado explícito;
- manifesto de build/promoções e snapshot de saúde versionados;
- logs estruturados locais, sem conteúdo sensorial bruto por padrão;
- modos finitos de verificação e replay para testes determinísticos;
- instalação CMake/CPack sem arquivos Python.

O host não inicia sensores concretos, modelos, atuadores, interface gráfica,
serviços em segundo plano ou conexões de rede nesta fase. A operação contínua
será possível somente quando uma SPEC posterior definir sua política de
processo, permissões e recuperação.

## Consequências

Positivas:

- existe uma fronteira operacional clara entre binário, capacidades e estado;
- o runtime pode ser testado sem hardware ou modelo;
- falhas de inicialização e degradação ficam observáveis;
- o mesmo host pode receber componentes promovidos sem importar Python.

Custos:

- ainda não há experiência de usuário final;
- a primeira operação depende de comandos explícitos;
- manifestos e snapshots passam a ser contratos versionados.

## Plano de reversão

Desabilitar o `RuntimeHost` e manter o executável de verificação de fixture.
Os contratos de evento, capacidades e timeline permanecem válidos e nenhum
estado persistido deve ser apagado durante a reversão.
