# Auditoría del 18 de octubre: operandos bajo control cuántico

La corrección integrada conserva la evaluación de aritmética CPU pura en la expresión cuántica
seleccionada. Evita ejecutar divisiones de ramas inactivas, protege el dominio de XOR y llenado, y
conserva las definiciones necesarias para la inversa. Los absolutos enteros estándar se reconocen
por la declaración o builtin real. Los homónimos privados conservan su semántica C.

La revisión v4 añadía versiones a actualizaciones reversibles que no las necesitaban. Dos casos de
la suite nativa fallaban: XOR booleano y bucle estático controlado. V5 conserva el camino de
actualización sobre el mismo registro cuando no hay un cálculo CPU diferido que requiera una prueba
de dominio guardada. Los
[fallos de v4](audit-evidence/controlled-operands-regression-failure/README.es.md) permanecen
disponibles con sus fuentes y registros.

## Evidencia de las fuentes actuales

La [campaña completa de 138 entradas](audit-evidence/controlled-operands-full/summary.json) terminó
con código 0: 76 comandos cerrados, reconstrucciones normales y ASan/UBSan, todas las suites
recurrentes, 665 compilaciones de fuzzing por configuración, 24 circuitos de crecimiento y 70
circuitos independientes con QuEST por configuración, 64 estados y 6 canales explícitos. El corpus
de 133 índices se ejecutó en tramos, conservando generador, programas, referencia C, cinco niveles,
los 256 valores de byte y semilla 20261012. Es un agregado explícito de comandos, sin atribuirlo a
una sola invocación original de `scripts/verify.sh`.

La [integración separada](audit-evidence/controlled-operands-integration/integration-summary.json)
reconstruyó ambas configuraciones y pasó 393 casos nativos, 1270 controles de operandos y los cuatro
programas exactos del [contrato](c11-controlled-operands.md) en cinco niveles de optimización y
ambos modos de builtins: 40 ejecuciones por configuración. La aplicación a Windows comprueba las
mismas 138 entradas; no afirma repetir allí toda la campaña Linux.

La [cápsula de estados](audit-evidence/controlled-operands-v5/summary.json) conserva 8000 estados
complejos e inversas de operandos, con capturas CPU reales y simulación dispersa independiente,
además de 1200 estados y 600 ejecuciones nativas de XOR y bucles. El máximo de 34 cables de las
exportaciones de operandos no se presenta como ejecución densa nativa por encima de su límite de 24
qubits. El fallo de coordinación del primer controlador se conserva junto a la continuación
instrumentada; no se inventa un código de salida para el proceso original.

## Pendientes que las regresiones no eliminan

La evaluación diferida del ángulo de `phase` y las cargas CPU de ramas inactivas siguen fallando. La
[matriz de cargas](audit-evidence/controlled-operands-v5/pending-cpu-loads.json) conserva 140
ejecutables aceptados que fallan, frente a 70 referencias C correctas. Sus siete perfiles cubren
asignación, XOR, suma, `memset`, `phase`, condiciones e índices con aritmética inválida. El
compilador integrado sigue afectado por esos casos, aun con las regresiones anteriores exitosas.

El inventario C11 conserva 1224 entradas: 782 pendientes, 74 parciales y 368 de metadatos. Las 27
funciones parcialmente adaptadas no se consideran implementaciones completas. Quedan flotantes,
complejos, memoria dinámica, punteros y ABI generales y otras familias. No se afirma ejecución en un
procesador cuántico físico ni aceleración. La ciberseguridad permanece clásica.
