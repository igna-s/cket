# Memoria por bytes sobre heaps cuánticos nativos

Campaña finita cerrada el **24 de octubre de 2026**. Se implementan `memcpy` y `memmove` entre heaps
y `memset` sobre heaps, con tamaños CPU de ejecución, bytes parciales, palabras enteras de distinto
tamaño y control coherente. El [contrato](../../c11-heap.md) conserva los límites y los pendientes.
No se declara completo C11 ni el perfil cotidiano.

La carga permanece en registros del motor; las direcciones host son marcadores clásicos. Copiar
calcula correlaciones de valores de base con el origen, sin crear una copia independiente de un
estado desconocido. `memmove` conserva las versiones originales antes de escribir; `memset` captura
su byte una vez. No se introducen mediciones ni resets.

| Comprobación                                           | Resultado y alcance                                                                                                                           |
| ------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------- |
| Estados complejos completos nuevos                     | 256 puntos: 192 normales (`O0`, `O2`, `Os`) y 64 con ASan/UBSan (`O1`), ambas políticas de síntesis. Son perfiles finitos repetidos.          |
| Inversión independiente de todo el registro de puertas | 64 recorridos completos que recuperan el estado inicial.                                                                                      |
| Programas nativos adicionales                          | 32 ejecuciones de ocho casos: alias/retornos, direcciones, funciones con préstamos, bucles CPU, palabras con signo y rangos vacíos.           |
| Rechazos semánticos/dominio                            | 48 comprobaciones de doce casos; los rechazos no cuentan como implementación.                                                                 |
| Motor del heap                                         | 16808 aserciones mixtas por configuración, incluidos ambos órdenes de bytes y siete esquemas nuevos inválidos antes de síntesis/asignaciones. |
| Regresión anterior del heap                            | 58 puntos de estado, quince programas válidos y diecisiete rechazos por configuración.                                                        |
| Núcleo nativo                                          | 395 comprobaciones normales aprobadas.                                                                                                        |
| Entrega usable                                         | Runner 05: `dynamic_heap`, `coherent_heap`, `heap_bytes`; el último también ejecuta con sanitizadores.                                        |

Cada punto compara el estado conjunto complejo con una fórmula independiente de enteros/fases y con
un intérprete independiente de las puertas emitidas. Las fórmulas no toman sus valores de los
certificados del motor. Incluyen historia retenida y resultados vivos después de liberar ambos
buffers, antes y después de su inversa guardada. El motor comprueba el orden de bytes grande aunque
el destino nativo de la campaña sea x86-64.

ASan/UBSan instrumentan plugin, driver, motor, runtime, programas y adaptador. `nm` confirma la
instrumentación. GCC host no está instrumentado; el wrapper existente desactiva la atribución de
fugas de ese proceso. Los ejecutables conservan su detección de fugas. No se repiten todas las
suites históricas.

Hay **206 entradas de fuente** identificadas en [summary.json](summary.json). `evidence.tar.gz`
contiene esas fuentes, casos, comandos, códigos de salida, estados, puertas y logs.
`tool-sha256.json` identifica los componentes construidos. Se conservan ambos manifiestos: después
de compilar solo se corrigió un caso negativo del validador para medir/liberar sus entradas antes de
salir. Ninguna fuente del compilador, motor o runtime cambió; se reutilizaron esos binarios.
`earlier-attempts.tar.gz` conserva los prototipos y ese intento detenido, con su alcance original.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
python3 scripts/validate-heap-memory.py --output build/heap-byte-proof --levels O0 O2 Os
build/validate_heap
bash scripts/run-block.sh 05
```

Siguen pendientes heap/buffers fijos, cadenas sobre heaps, punteros/ABI generales, resultados de
actualizaciones de heap en bucles CPU y flotantes cuánticos. Se admiten enteros de bytes completos,
hasta 64 bytes por operación y los presupuestos físicos/de historia separados. Liberar no borra
salidas desconocidas. No se reauditan aquí los papers ni se acredita una QPU física o aceleración.
El inventario conserva 1224 entradas, 94 parciales y ninguna interfaz completa.
