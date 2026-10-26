# Integración C prioritaria y referencia QASM

26 de octubre de 2026. Candidata experimental `1.0.0-rc.3`; no es el cierre de la 1.0 estable.

Se corrige una expectativa del fixture QASM: una rama CPU puede seleccionarse mediante su traza sin
necesitar un parámetro numérico `qbind`. La referencia sigue comprobando la cantidad de parámetros
de cada `qbind` emitido y exige bindings para los fixtures aritméticos sin traza CPU. El compilador,
plugin, motor y runtime no cambian respecto del SDK publicado en `440e7e9`.

Las 237 entradas se congelan de nuevo. Solo cambia `tests/validate_qasm.c`, incluida su presentación
con el formateador y la guarda que conserva los tokens al formatear. Los otros validadores tienen
las mismas dependencias locales que sus binarios instrumentados anteriores; la evidencia registra
esas dependencias y los hashes de los ejecutables reutilizados.

## Resultado de los validadores C

Cada fila pasa en normal. Diez validadores también completan ASan/UBSan, según la última columna.
Los conteos mezclan ámbitos explícitos en sus registros: ejecuciones nativas, estados completos,
inversas, límites y rechazos. No representan porcentajes de C ni un número de funciones
implementadas.

| Validador                           | Comprobaciones | ASan/UBSan en esta campaña |
| ----------------------------------- | -------------: | -------------------------- |
| Operandos controlados               |           1270 | Pendiente                  |
| Fases                               |           3380 | Pasa                       |
| Capturas de bytes CPU               |            650 | Interrumpida por tiempo    |
| División                            |            990 | Pendiente                  |
| Bindings CPU                        |            519 | Pasa                       |
| Historial de versiones              |           2332 | Pasa                       |
| Heap                                |          16878 | Pasa                       |
| Identidades de heap                 |           1682 | Pasa                       |
| Préstamos inactivos                 |            139 | Pasa                       |
| Swaps e inversas                    |          35871 | Pasa                       |
| Parches de registros                |          55143 | Pasa                       |
| Versiones del runtime               |             15 | Pasa                       |
| Preflight de memoria                |             23 | Pasa                       |
| Clasificación y conversión de bytes |           1815 | Pendiente                  |
| Perfiles inmutables                 |             96 | Pendiente                  |
| Locale numérico                     |             70 | Pendiente                  |
| Frontera de libc                    |           2100 | Pendiente                  |

Los 2100 casos de frontera prueban diagnósticos en 210 interfaces; **no son 210 adaptadores
cuánticos implementados**. Retornos y distancias tienen además sus resultados normales e
instrumentados en la [cápsula anterior](../memory-return-reference-v1/README.es.md).

## Exportación y comprobaciones adicionales

La pasada normal completa 145 compilaciones del fuzzer, semilla 20261012: 24 casos generados y cinco
fijos, en cinco niveles GCC. Cada compilación se contrasta en los 256 valores de byte. No es la
campaña histórica de 665 compilaciones. También pasan 24 circuitos de crecimiento sin tablas y cinco
comparaciones clásicas de integración que incluyen las 29 familias de headers C11. Incluir un header
clásico no implementa su equivalente cuántico.

QuEST 3.7.0 comprueba 70 circuitos exportados sin tablas: 64 estados completos y seis casos de
medición/reset. Los estados se contrastan con el motor nativo, hasta una fase global, con tolerancia
de amplitud de `1e-10`; esto prueba consistencia de exportación y ejecución, no por sí solo la
semántica C del programa. Los seis casos comparan distribuciones de 8192 transcripciones por backend
con tolerancia estadística; no son una prueba exacta de todos los canales posibles. Esta pasada QASM
es normal; no se presenta como una pasada QuEST instrumentada. El control separado de locale
confirma que la exclusión específica de glibc conserva la detección de fugas del proyecto y de
handles de locale.

## Reproducción y límites

`evidence.tar.gz` conserva fuentes, identidades, comandos, hashes, logs y harnesses. Conserva el
fallo inicial de la expectativa QASM y dos problemas de preparación del harness: una ruta de
dependencia `tests/../src` sin normalizar y un ejecutable auxiliar ausente en la primera copia del
entorno QuEST. Las continuaciones corrigen la preparación y tienen resultados terminales separados;
esos fallos de infraestructura no cuentan como rechazos semánticos.

El cierre de tiempo detiene la pasada instrumentada durante capturas de bytes para publicar la
entrega dentro de la cuota de la sesión. Su log parcial no cuenta como éxito ni como fallo
semántico. Los seis validadores instrumentados que venían después no se ejecutan en esta campaña.

Esta campaña **no ejecuta todo `scripts/verify.sh`**: faltan su composición completa, matrices
Python dentro de esa ejecución y los demás alcances que no aparecen arriba. Las matrices de
coherencia publicadas anteriormente conservan sus fuentes y límites históricos. Siguen abiertos
vidas, alias y ABI generales, flotantes cuánticos, perfiles excluidos por recursos y la revisión
completa de los manuscritos generales. No se afirma QPU física ni aceleración. La ciberseguridad
permanece clásica y sin cambios.
