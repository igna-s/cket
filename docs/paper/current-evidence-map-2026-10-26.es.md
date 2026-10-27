# Cómo leer los papers junto con la entrega actual

Actualizado el 27 de octubre de 2026. Esta nota relaciona las afirmaciones y las fuentes de
evidencia; no es una recalificación completa de los manuscritos ni cambia sus PDF históricos.

Los dos manuscritos generales estudian el prototipo fijado en
`1ebb2e308e61bed5632798ded5beecf77b6b9a9c`. Sus cifras de 354 pruebas nativas, 426 comprobaciones de
16 programas y 11 circuitos con QuEST pertenecen a esa evaluación. No describen la cobertura del
compilador actual. Las notas posteriores y sus cápsulas identifican otras fuentes y otros ámbitos.
Una cifra mayor no convierte una campaña anterior en evidencia del nuevo compilador.

| Afirmación                                       | Evidencia disponible y límite                                                                                                                                                                                                                                                                                                                                                                 |
| ------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Se puede usar el compilador actual.              | El [SDK de llamadas coherentes](../../output/sdk/loop-jumps/README.es.md), con 244 entradas, ejecuta 37 programas; su suite registra 1509 comprobaciones normales. Los lanzadores Windows ejecutan el paquete y compilan ejemplos de retornos/interferencia y heap. Esto prueba esos programas y flujos, no todo C.                                                                           |
| Las copias respetan la no clonación.             | Los [contratos de propiedad](../no-clonacion.es.md) distinguen movimiento, préstamo y correlación en la base computacional. Las cápsulas de heap, buffers y cadenas contrastan densidades, estados conjuntos e inversas de perfiles finitos. No prueban todos los alias y vidas.                                                                                                              |
| Se conserva el historial necesario.              | La [referencia de retornos](../audit-evidence/memory-return-reference-v1/README.es.md) rechaza una historia redundante que copia un byte intacto. No exige identificadores separados para el dueño anterior y el actual. Los casos de palabras comprueban la inversa completa de puertas, no una inversa guardada del patch emitida por el compilador.                                        |
| La limpieza de temporales funciona.              | La [campaña C](../audit-evidence/c-priority-integration-v1/README.es.md) completa 2332 comprobaciones de historial normales e instrumentadas. Las pruebas identifican qué inversas y estados verifican. Esto no demuestra limpieza para cualquier flujo C ni que todo historial desconocido pueda liberarse.                                                                                  |
| La exportación QASM es correcta.                 | La misma campaña contrasta 64 estados completos de exportación con el motor nativo, hasta una fase global, con tolerancia de amplitud `1e-10`. Otros seis casos comparan distribuciones de medición/reset con 8192 transcripciones por backend. Es consistencia entre implementaciones en esos casos, no un oráculo independiente de semántica C ni una prueba exacta de canales arbitrarios. |
| La biblioteca C tiene compatibilidad cuántica.   | Hay adaptadores parciales, descritos en el [perfil cotidiano](../perfil-c-cotidiano.es.md). Los 2100 diagnósticos sobre 210 interfaces de libc prueban la frontera de rechazo; no implementan esas 210 interfaces sobre datos cuánticos.                                                                                                                                                      |
| El sistema ejecuta operaciones cuánticas reales. | El compilador genera puertas y canales cuánticos y el simulador C evoluciona amplitudes complejas. No se ha demostrado ejecución en una QPU física. El número de cables de un circuito o de qubits de un simulador no es una cantidad de qubits de hardware utilizados.                                                                                                                       |
| Los recursos indican el costo de hardware.       | Los conteos son lógicos: una MCX se cuenta antes de su descomposición física. No incorporan conectividad, ruido ni corrección de errores. En el prototipo histórico, «qubits físicos» de la tabla de simulación designa cables asignados del modelo ideal, no un procesador físico. No hay demostración de aceleración frente a CPU.                                                          |

La campaña de 237 entradas tiene 17 validadores C normales aprobados y diez completos con
ASan/UBSan. La
[continuación de perfiles y locale](../audit-evidence/c-locale-sanitized-v1/README.es.md) añade dos
instrumentados; quedan cinco de aquella campaña. Las 70 comprobaciones QuEST de esas fuentes son
normales. Estos resultados no recalifican automáticamente los cambios posteriores.

Las campañas nuevas de [préstamos de lectura](../audit-evidence/readonly-borrows-v1/README.es.md),
con 239 entradas, y [entradas de funciones](../audit-evidence/function-entries-v1/README.es.md), con
240, identifican sus fuentes y regresiones normales e instrumentadas. Se distinguen las llamadas
cuánticas comprobadas de una ABI clásica que podía terminar sin ejecutar la operación.

La [referencia de memoria corregida](../audit-evidence/runtime-memory-reference-v1/README.es.md)
cierra el fallo reproducido `unmodelled retained version`, con 900 puntos de estado complejo y 270
inversas completas. Solo cambia un validador Python; el SDK conserva sus fuentes y hashes
originales. La [continuación de memoria](../audit-evidence/memory-integration-tail-v1/README.es.md)
añade 540 puntos y 210 inversas de escrituras coherentes y bucles fijos, con los mismos productores.
Los alcances se solapan con campañas anteriores y no se suman como cobertura independiente.

La [integración completa de `ac8f5bc`](../audit-evidence/full-ci-cpu-returns-v1/README.es.md), de
242 entradas, y la de
[llamadas coherentes `cba9759`](../audit-evidence/full-ci-coherent-returns-v1/README.es.md), de 244
entradas, terminaron aprobadas. Ambas conservan logs, artefactos e identidades Git. El CI de
`183169a` también terminó aprobado. No recalifican las revisiones posteriores de locales, bucles o
saltos; una misma cantidad de archivos no identifica las mismas fuentes.

Siguen pendientes el cierre del perfil cotidiano, vidas/alias/ABI generales, flotantes cuánticos,
perfiles excluidos por recursos, integración de los cambios posteriores y revisión científica
general de los papers. La candidata continúa siendo `1.0.0-rc.3`. La ciberseguridad permanece
clásica.

La [ampliación de locales CPU privados](private_cpu_locals_note.es.md) también tiene 244 entradas,
con hashes distintos de `cba9759`. Su
[campaña dirigida](../audit-evidence/immutable-cpu-locals-v1/README.es.md) y su SDK comprueban esa
ampliación; el CI enlazado de `cba9759` corresponde a su predecesor. Ni la cantidad de archivos ni
el CI de una revisión sustituyen la identidad de fuentes de la otra.

El SDK recomendado ahora usa la [ampliación de bucles CPU](private_cpu_loops_note.es.md), con 244
entradas y otros hashes. Su [evidencia](../audit-evidence/private-cpu-loops-v1/README.es.md)
comprueba locales mutables privados, condiciones CPU y bucles/retornos anidados con estados e
inversas. La condición no puede ser cuántica; quedan saltos y vidas generales. Los CI anteriores
corresponden a sus revisiones originales; el CI completo de esta ampliación sigue pendiente.

El SDK recomendado incluye ahora [break/continue CPU](loop_jumps_note.es.md), con otras 244
identidades de fuente. Su [cápsula](../audit-evidence/loop-jumps-v1/README.es.md) distingue 1100
comparaciones C normales de 1900 puntos de estado con llamador coherente y sus inversas; ambas
matrices pasan también instrumentadas con sus propios recuentos. Quedan saltos cuánticos,
flujo/vidas generales e integración completa de estas fuentes.

La [auditoría de memoria con saltos](memory_loop_jumps_note.es.md) añade 245 entradas de campaña,
con fuentes de producción y binarios iguales a `5f12080`. Su
[cápsula](../audit-evidence/memory-loop-jumps-v1/README.es.md) registra estados/inversas y
densidades 2×2 del bit preparado en el byte intacto; no es una prueba de estados arbitrarios de un
byte ni de estas familias con un llamador coherente exterior. La integración nueva permanece
pendiente de su CI completo.
