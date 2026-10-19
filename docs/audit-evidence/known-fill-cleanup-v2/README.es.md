# Limpieza de temporales conocidos en `memset`

Esta integración conserva una definición invertible para los destinos enteros conocidos que no se
observan ni transforman en la función. El compilador deshace esos temporales al terminar su vida.
Los destinos que alimentan observaciones o transformaciones conservan la ruta mutable, incluido el
orden de medición previo. La evaluación CPU se captura una vez y su inversa utiliza la captura
original.

Las 146 entradas pasaron, por configuración normal y ASan/UBSan, 940 comprobaciones de `memset`, 393
casos nativos, 1270 controles de operandos, 2332 de historia, 271 actualizaciones y 360 expresiones
de memoria. Se añadieron 14 fixtures mantenidos que producen 140 ejecuciones nativas adicionales por
configuración. Dos segmentos conservan los cierres anteriores y repiten solo las suites sin estado
terminal tras el cambio de arranque de WSL. No se inventa un código de salida para los procesos
interrumpidos ni se afirma una ejecución única de `scripts/verify.sh`.

Una prueba independiente pasó 3528 estados complejos completos: 588 capturas y 1764 estados en cada
configuración, con 294 referencias C por configuración. Los checkpoints solo exportan puertas y
metadatos. Las asignaciones y llenados no inyectivos utilizan las inversas emitidas por el
compilador; XOR, suma y efectos de fase tienen inversas explícitas en el programa C de prueba. Las
entradas desconocidas retenidas permanecen como salida conjunta. La prueba comprueba fases
distintas, cargas CPU válidas o ramas inactivas, mutaciones CPU posteriores y ceros en los
temporales.

Otra prueba verificó un prefijo de 600 estados con valores negativos de 8, 16 y 32 bits, pero
**terminó con error** en el llenado parcial de 64 bits por el límite de 128 qubits físicos. Los dos
cierres fallidos, sus fuentes y su prefijo positivo quedan en `signed-resource-limit.json` y el
archivo de evidencia. No se cuenta esa prueba ancha como aprobada.

`sources.tar.gz` contiene las fuentes exactas. `scoped-evidence.tar.gz` conserva referencias,
circuitos, mapas, hashes, controladores y logs. `earlier-attempts.tar.gz` conserva los errores de
los primeros oráculos, el rechazo original de limpieza y el candidato anterior que emitía una
constante con signo fuera de su tipo. No se atribuyen esos errores del candidato a la versión
original.

La aplicación por hashes al repositorio no equivale a una regresión ejecutada en Windows. La campaña
completa y el CI de estas 146 entradas siguen pendientes. El CI anterior aprobado corresponde a144
entradas y se conserva por separado. C11 completo, hardware físico y aceleración siguen sin
demostrarse. La ciberseguridad permanece clásica.

La aplicación en la raíz Linux se comprobó con otra reconstrucción normal y 20 ejecuciones de los
dos programas exactos: `cleanup-interference.c` y `observation-order.c`, en los cinco niveles GCC y
ambos modos de builtins. `root-checks.json` y `root-checks.tar.gz` conservan los cierres y las
fuentes. Son controles de aplicación, separados de las suites normales e instrumentadas del
candidato.
