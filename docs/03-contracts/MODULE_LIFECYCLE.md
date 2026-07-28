# Contrato de Ciclo de Vida de Módulos

Todo plugin removível deve implementar:

```text
discover()
validate_manifest()
configure()
initialize()
calibrate()
health_check()
start()
pause()
resume()
drain()
checkpoint()
stop()
uninstall()
```

Nem todo método precisa produzir estado persistente, mas deve responder de forma tipada.

## Garantias

- `start()` é idempotente.
- `stop()` não apaga memória histórica.
- `drain()` impede novas operações e aguarda as atuais dentro de timeout.
- `checkpoint()` é obrigatório para módulos com estado aprendível.
- falha de um plugin não encerra o processo cognitivo;
- eventos de ciclo de vida entram na timeline;
- operações em andamento possuem correlation ID;
- remoção invalida handles antigos;
- reinstalação requer nova validação e calibração.
