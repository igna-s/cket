# Copias entre buffers fijos y heaps cuánticos

Campaña finita cerrada el **24 de octubre de 2026**. `memcpy` y `memmove` copian entre un heap
entero cuántico y un buffer cuántico fijo en ambas direcciones. Objetos CPU conocidos y no volátiles
pueden aportar bytes capturados al heap. Las vistas fijas conservan direcciones comprobadas; la
longitud puede ser CPU de ejecución. El [contrato](../../c11-heap.md) mantiene los límites y
contextos pendientes.

La copia conserva el origen y las versiones necesarias para las inversas. No duplica un estado
desconocido de forma independiente. Los bytes se calculan desde registros reales o capturas CPU, sin
cargar el contenido de un marcador host del heap. La ruta heap→fijo usa las versiones del compilador
y uniones CPU. Las palabras fuera de la longitud conservan sus qubits; una longitud cero conserva
todos los qubits públicos del destino.

| Comprobación                                 | Resultado y alcance                                                                                                                                                                                   |
| -------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Estados complejos completos nuevos           | 224 puntos: 168 normales (`O0`, `O2`, `Os`) y 56 con ASan/UBSan (`O1`), ambas políticas de síntesis. Son perfiles finitos repetidos.                                                                  |
| Inversión independiente de todas las puertas | 56 recorridos completos hasta el estado inicial.                                                                                                                                                      |
| Casos nativos adicionales                    | 20 ejecuciones de cinco casos: capturas CPU contadas, bucle CPU→heap, alias del retorno fijo, recorrido escalar de ida/vuelta y conservación del sufijo.                                              |
| Rechazos semánticos/dominio                  | 28 comprobaciones de siete casos; no cuentan como implementación.                                                                                                                                     |
| Motor del heap                               | 16878 aserciones mixtas por configuración, con ocho esquemas nuevos inválidos antes de síntesis/asignaciones y cuatro puntos de estado/fase de lectura de bytes e inversa, en ambos órdenes de bytes. |
| Regresión de bytes del heap                  | 64 puntos, 16 inversas, nueve programas y once rechazos por configuración.                                                                                                                            |
| Regresión anterior del heap                  | 58 puntos, quince programas y diecisiete rechazos por configuración.                                                                                                                                  |
| Núcleo nativo                                | 395 comprobaciones normales aprobadas.                                                                                                                                                                |
| Entrega usable                               | Runner 05: cuatro ejecutables nativos; `heap_transfer` también ejecuta con sanitizadores.                                                                                                             |

Cada punto compara el estado conjunto complejo con fórmulas independientes de enteros/fases y con un
intérprete independiente de las puertas emitidas. Se incluyen los originales retenidos, guardados
coherentes, modificaciones posteriores del origen y resultados vivos después de liberar el heap. Las
fórmulas no toman sus valores de certificados del motor. El rango vacío comprueba además la
identidad de los cables públicos. El adaptador cuenta lecturas CPU habilitadas y registra nombres a
través de las uniones de dueño; no sustituye operaciones ni cargas.

ASan/UBSan instrumentan plugin, driver, motor, runtime, programas y adaptador. `nm` comprueba esa
instrumentación. GCC host no está instrumentado; su wrapper conserva la exclusión existente de
atribución de fugas del proceso host. Los ejecutables conservan su detección de fugas.

[summary.json](summary.json) identifica **210 entradas congeladas**: las mismas en ambas
construcciones limpias y en las comprobaciones finales. `evidence.tar.gz` contiene fuentes, casos
generados, estados, puertas, comandos, códigos de salida y logs. `tool-sha256.json` identifica los
componentes construidos. `earlier-attempts.tar.gz` conserva los prototipos y sus resultados, con su
alcance original; sus fuentes completas de compilador no están congeladas.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
python3 scripts/validate-heap-transfer.py --output build/heap-transfer-proof --levels O0 O2 Os
build/validate_heap
bash scripts/run-block.sh 05
```

Siguen pendientes las escrituras heap→fijo dentro de bucles CPU, direcciones fijas generales,
consumidores de puntero/ABI indirecta, cadenas sobre heaps y flotantes cuánticos. Se aplican los
límites de bytes, qubits físicos, historia y síntesis por separado. Las salidas desconocidas
retiradas no desaparecen al liberar. Esta campaña no repite todas las suites históricas, reaudita
los papers ni demuestra uso de una QPU física o aceleración. El inventario sigue incompleto.
