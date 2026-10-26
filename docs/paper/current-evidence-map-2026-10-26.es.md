# Cómo leer los papers junto con la entrega actual

26 de octubre de 2026. Esta nota relaciona las afirmaciones y las fuentes de evidencia; no es una
recalificación completa de los manuscritos ni cambia sus PDF históricos.

Los dos manuscritos generales estudian el prototipo fijado en
`1ebb2e308e61bed5632798ded5beecf77b6b9a9c`. Sus cifras de 354 pruebas nativas, 426 comprobaciones de
16 programas y 11 circuitos con QuEST pertenecen a esa evaluación. No describen la cobertura del
compilador actual. Las notas posteriores y sus cápsulas identifican otras fuentes y otros ámbitos.
Una cifra mayor no convierte una campaña anterior en evidencia del nuevo compilador.

| Afirmación                                       | Evidencia disponible y límite                                                                                                                                                                                                                                                                                                                                                                 |
| ------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Se puede usar el compilador actual.              | El [SDK](../../output/sdk/README.es.md) ejecuta 37 programas; su suite registra 1509 comprobaciones normales. Los lanzadores Windows ejecutan el paquete y compilan dos archivos C independientes. Esto prueba esos programas y flujos, no todo C.                                                                                                                                            |
| Las copias respetan la no clonación.             | Los [contratos de propiedad](../no-clonacion.es.md) distinguen movimiento, préstamo y correlación en la base computacional. Las cápsulas de heap, buffers y cadenas contrastan densidades, estados conjuntos e inversas de perfiles finitos. No prueban todos los alias y vidas.                                                                                                              |
| Se conserva el historial necesario.              | La [referencia de retornos](../audit-evidence/memory-return-reference-v1/README.es.md) rechaza una historia redundante que copia un byte intacto. No exige identificadores separados para el dueño anterior y el actual. Los casos de palabras comprueban la inversa completa de puertas, no una inversa guardada del patch emitida por el compilador.                                        |
| La limpieza de temporales funciona.              | La [campaña C](../audit-evidence/c-priority-integration-v1/README.es.md) completa 2332 comprobaciones de historial normales e instrumentadas. Las pruebas identifican qué inversas y estados verifican. Esto no demuestra limpieza para cualquier flujo C ni que todo historial desconocido pueda liberarse.                                                                                  |
| La exportación QASM es correcta.                 | La misma campaña contrasta 64 estados completos de exportación con el motor nativo, hasta una fase global, con tolerancia de amplitud `1e-10`. Otros seis casos comparan distribuciones de medición/reset con 8192 transcripciones por backend. Es consistencia entre implementaciones en esos casos, no un oráculo independiente de semántica C ni una prueba exacta de canales arbitrarios. |
| La biblioteca C tiene compatibilidad cuántica.   | Hay adaptadores parciales, descritos en el [perfil cotidiano](../perfil-c-cotidiano.es.md). Los 2100 diagnósticos sobre 210 interfaces de libc prueban la frontera de rechazo; no implementan esas 210 interfaces sobre datos cuánticos.                                                                                                                                                      |
| El sistema ejecuta operaciones cuánticas reales. | El compilador genera puertas y canales cuánticos y el simulador C evoluciona amplitudes complejas. No se ha demostrado ejecución en una QPU física. El número de cables de un circuito o de qubits de un simulador no es una cantidad de qubits de hardware utilizados.                                                                                                                       |
| Los recursos indican el costo de hardware.       | Los conteos son lógicos: una MCX se cuenta antes de su descomposición física. No incorporan conectividad, ruido ni corrección de errores. En el prototipo histórico, «qubits físicos» de la tabla de simulación designa cables asignados del modelo ideal, no un procesador físico. No hay demostración de aceleración frente a CPU.                                                          |

La pasada más reciente tiene 17 validadores C normales aprobados y diez completos con ASan/UBSan. La
pasada instrumentada de capturas de bytes se interrumpió por tiempo; los seis validadores que venían
después no se ejecutaron. Las 70 comprobaciones QuEST de esa campaña son normales. No debe
presentarse como una ejecución completa de `scripts/verify.sh`.

Siguen pendientes el cierre del perfil cotidiano, vidas/alias/ABI generales, flotantes cuánticos,
perfiles excluidos por recursos, integración completa y revisión científica general de los papers.
La candidata continúa siendo `1.0.0-rc.3`. La ciberseguridad permanece clásica.
