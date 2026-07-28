# Questões Abertas

1. Sistema operacional inicial: Windows 11 confirmado?
2. GPU dedicada ou integrada disponível?
3. Limite aceitável de armazenamento diário?
4. Frequência máxima de captura visual?
5. Modelo multimodal local definitivo?
6. Banco de dados inicial: resolvido pela SPEC-006 como SQLite puro; Qdrant e
   outros armazenamentos semânticos permanecem fora do escopo inicial.
7. Interface do avatar: Tauri, Qt ou outra?
8. Idioma interno canônico: português ou inglês?
9. Política de retenção de áudio bruto?
10. Métrica humana para utilidade de perguntas?

11. Formato inicial de plugins: entry points Python, manifests em diretório ou subprocessos?
12. Hot-plug será exigido na primeira versão ou apenas restart-safe?
13. Quais operações formam a ontologia inicial de capacidades?
14. Como versionar modalidades novas sem alterar `CanonicalEvent`?
15. Quais sensores serão considerados obrigatórios? Recomendação atual: nenhum sensor de domínio; somente relógio, event bus e estado interno.


## Resoluções registradas pela SPEC-023

11. A primeira implementação suporta os dois mecanismos locais: manifestos
   JSON em diretório e entry points Python. Subprocessos permanecem fora desta
   SPEC.
12. Hot-plug é exigido quando o descritor declara suporte; estados persistidos
   e checkpoints tornam o reinício seguro nos demais casos.
13. Não há ontologia fixa de operações: cada `CapabilityDescriptor` declara as
   operações que fornece, e o resolver seleciona por operação.
