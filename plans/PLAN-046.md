# Plano de Execução: SPEC-046 (Consistent Cognitive Snapshot and Replay)

## Pré-condições
- SPEC-045 implementada.
- SQLite `PrivacyStorage` operacional.

## Contratos Congelados
- `CanonicalEvent`.

## Arquivos por Etapa

### Etapa 1: Definição do Schema e Contrato
- **Ação:** Escrever o schema validável.
- **Arquivos:**
  - `contracts/schemas/cognitive_snapshot.schema.json`
  - `cpp/core/contracts/cognitive_snapshot.hpp`

### Etapa 2: Acesso SQLite Atomic
- **Ação:** Extender o storage para gravação de par chave-valor com transação atômica.
- **Arquivos:**
  - `cpp/core/privacy_storage.hpp` (adicionar métodos `save_snapshot`, `load_last_snapshot`)
  - `cpp/core/privacy_storage.cpp`
- **Testes antes do código:** Teste garantindo que um erro no meio da serialização causa rollback da transação SQLite.

### Etapa 3: Integração no Coordenador
- **Ação:** Disparar em background a criação do clone. Implementar a rotina de Replay Fast-Forward no `start()` do `RuntimeHost`.
- **Arquivos:**
  - `cpp/core/cognitive_coordinator.cpp`
  - `cpp/core/runtime_host.cpp`

## Comandos de Validação
```bash
cmake --build build/windows-msvc --target all
ctest --test-dir build/windows-msvc -R SnapshotReplay --output-on-failure
```

## Migrações
- Adicionar instrução `CREATE TABLE IF NOT EXISTS cognitive_snapshots` no script de subida do DB (`cpp/core/privacy_storage.cpp`).

## Rollback
- Reverter o PR, omitindo a chamada de leitura de snapshot, sem necessidade de dropar tabelas à força no banco de clientes atuais (não intrusivo).

## Evidências Esperadas
- Dump hexadecimal/json do sqlite comprovando o Blob e os hashes.
- Teste end-to-end gravando evento 1..100, snapshot, matando processo, gravando evento 101..120, desligando. Ao ligar, ele pega o snapshot 100 e processa fast-forward 101..120.

## Critérios para Parar
- Se o clone assíncrono para o snapshot causar bloqueio de mutex no `GlobalWorkspace` por mais de 5ms (stutter), revisar arquitetura para copy-on-write.
