# Integración de cadenas, ángulos y cargas CPU: 144 entradas

La integración añade adaptadores parciales de `strlen` y `strcmp`, ángulos clásicos diferidos de
`phase` y protección de lecturas CPU bajo controles probadamente inactivos. La consulta de controles
usa metadatos y certificados de valores; no mide ni consulta amplitudes. La ciberseguridad permanece
clásica. No se modificó la configuración de Windows o WSL.

Cuando el comprobador no logra demostrar que los controles están inactivos, devuelve «posible» y se
captura el operando CPU. Su dirección y su evaluación deben ser válidas en ese caso. La protección
no promete reconocer toda inactividad que pueda resultar de interferencia cuántica.

Las pruebas dirigidas cerraron 18 comandos correctamente: dos builds frescos y ocho suites por
configuración. En normal y ASan/UBSan pasan 519 comprobaciones de bindings, 460 de cadenas, 393
nativas, 1270 de operandos controlados, 302 de `memcmp`, 246 de `strncmp`, 2100 de fronteras de
biblioteca y 3380 de ángulos. Los rechazos de fronteras no implementan las operaciones rechazadas.

Una prueba independiente de programas realmente compilados conserva 160 capturas, 80 ejecuciones de
referencias C ordinarias y 320 estados complejos completos por configuración: **640 estados en
total**, antes y después de inversas guardadas. Comprueba todas las amplitudes de sus dominios
finitos, entradas preservadas y cables privados en cero. Incluye resultados de 64 bits de `strlen`,
32 bits de `strcmp`, dos entradas cuánticas y objetos CPU modificados después de capturarlos. La
referencia de `strcmp` comprueba el signo con libc y calcula por separado la diferencia exacta de
los primeros bytes sin signo que difieren. No se exige a libc una magnitud que C no prescribe.

La matriz actual de cargas CPU pasa 140 filas, 420 ejecuciones nativas y 420 referencias C por
configuración: asignación, XOR, suma, `memset`, `phase`, condiciones e índices; cinco niveles de
optimización y ambos modos de builtins. Una prueba independiente adicional pasa **1120 estados
exportados**, con fases distintas por entrada, historia retenida y limpieza por inversa del
resultado derivado. La inversa de la propia actualización controlada y la limpieza de su historia
quedan separadas y pendientes. Los dos programas exactos del
[contrato de cadenas](../../c11-strings.md) pasaron 40 ejecuciones.

Las fuentes canónicas tienen 144 entradas. Las pruebas dirigidas usan los mismos fuentes C/C++,
cabeceras y tests; la única diferencia de sus mapas completos es `scripts/archive-audit.py`, que
actualiza el contador de bindings de 399 a 519. `sources.tar.gz` conserva el conjunto canónico;
`history.tar.gz` contiene ese archivo anterior para reconstruir exactamente el conjunto probado.
`scoped-evidence.tar.gz` incluye controladores, fuentes de pruebas, eventos, circuitos, mapas,
referencias y transcripciones. Los hashes internos se verifican antes de publicar.

Se conservan los fallos anteriores: v2 no pudo ejecutar una prueba SAN que su script invocaba antes
de compilarla; v3 volvió a rechazar el binding válido `n ? 1 / d : false` con `n` falso y `d` cero.
El árbol de GCC muestra que la comparación heredaba un tipo booleano `volatile` de la marca
cuántica. La corrección distingue resultados calculados de accesos a almacenamiento. Sus nuevos
casos siguen rechazando lecturas `volatile` reales e incrementos CPU bajo control coherente.

**La campaña amplia de 80 comandos y el CI remoto de esta integración siguen pendientes.** La
fotografía archivada identifica su prefijo cerrado, sin presentarlo como un cierre exitoso. Las
campañas completas y el CI de las 138 entradas anteriores conservan sus fuentes originales. Los
experimentos del paper también mantienen su revisión histórica.

El inventario tiene 1224 entradas: 780 pendientes, 76 parciales y 368 de metadatos. Las parciales
incluyen 29 nombres de funciones y 47 tipos o alias; ninguna entrada se declara completa. Los
adaptadores de cadenas requieren vistas enteras comprobadas de hasta 64 bytes con NUL en el dominio
evaluado. Quedan pendientes el diagnóstico específico de NUL, punteros y tamaños dinámicos, layouts
generales, llamadas indirectas, flotantes cuánticos y las demás familias de C11. Los límites de
síntesis y simulación son distintos del límite de bytes. No se afirma hardware ni aceleración.
