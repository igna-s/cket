# Avance de cadenas: implementación y ejemplos

Se añaden `strspn`, `strcspn` y los predicados directos con `NULL` de `memchr`, `strchr`, `strrchr`,
`strpbrk` y `strstr`. El [contrato](../../c11-string-searches.md) delimita los contextos
disponibles.

La fotografía contiene **168 entradas de compilador**. Una reconstrucción normal con GCC 11 terminó
con código 0. El runner del bloque 06 también terminó con código 0: ejecutó dos programas nativos y
generó dos circuitos OpenQASM y dos informes de recursos. Se observaron:

```text
string_spans: byte=252 accepted=0 rejected=1
string_search: byte=56 a=0 last=0 digit=1 substring=0 raw=0
```

Son ejecuciones concretas, no una comparación exhaustiva de estados. Por indicación del usuario se
prioriza implementar: quedan aplazadas las matrices completas de amplitudes/fases, inversas,
capturas CPU modificadas, contextos GCC, sanitizadores e integración general de este código nuevo.
No se reutilizan las pruebas de 164 entradas como si validaran las 168 actuales. Tampoco se afirma
ejecución en una QPU física o aceleración frente a CPU.

`summary.json` registra el estado incompleto de validación y las huellas de fuentes y artefactos.
`evidence.tar.gz` conserva las fuentes exactas, los logs de construcción/runner y los circuitos e
informes resultantes. La compilación incluye los ejecutables de tests existentes, pero no se
ejecutaron esas suites para este avance.
