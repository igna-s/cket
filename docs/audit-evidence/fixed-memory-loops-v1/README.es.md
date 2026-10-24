# Copias, movimientos y rellenos fijos en bucles CPU

Entrega finita del **24 de octubre de 2026**. `memcpy`, `memmove` y `memset` entre buffers enteros
fijos pueden ejecutarse dentro de bucles CPU estructurados, con tamaños y direcciones constantes
comprobados. Se incluyen movimientos solapados en ambos sentidos y bytes parciales. Cada escritura
retiene su dueño anterior y las dependencias necesarias para la inversa. Una prueba de base conocida
al entrar al bucle no permite reutilizarla en todas sus iteraciones.

Si el relleno lee su propio destino, se calcula una captura antes del primer cambio. Por ejemplo,
`memset(a, a[0] + 1, sizeof a)` llena todas las palabras con el mismo byte de esa llamada. La
captura tiene almacenamiento limpio en cada iteración y conserva sus originales hasta que terminan
sus consumidores. No hay mediciones, resets ni eliminación implícita de historia desconocida.

El [ejemplo usable](../../../examples/blocks/05/fixed_buffers.c) pasa un array a una función que
combina `memmove` y `memset` dentro de `while(rounds--)`. El contador por valor es local al callee.
El ejemplo acepta 0 a 3 iteraciones dentro de su presupuesto de historia y observa primero los
consumidores, después sus fuentes. El runner 05 incluye ahora seis ejecutables nativos.

| Comprobación                                  | Resultado y alcance                                                                                                                      |
| --------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Estados complejos completos nuevos            | 300 puntos: 200 normales (`O0`, `O2`) y 100 ASan/UBSan (`O1`), ambas políticas de síntesis.                                              |
| Inversión independiente de todas las puertas  | 90 recorridos completos hasta el estado inicial.                                                                                         |
| Perfiles                                      | Copia, movimiento solapado en ambos sentidos, relleno que lee su destino y copia de bytes parciales; 0, 1 y 3 iteraciones.               |
| Contextos nativos adicionales                 | 32 ejecuciones: argumentos CPU evaluados una vez, snapshots de bytes CPU, ramas CPU, identidad y el ejemplo con 0, 1, 2 y 3 iteraciones. |
| Copia mantenida por configuración             | 414 chequeos: 120 estados/inversas, 4 estados solapados, 190 contratos nativos, 70 rechazos y 30 generaciones anchas.                    |
| Relleno mantenido por configuración           | 940 chequeos con estados, caminos etiquetados, ejecuciones, rechazos y generación; sus categorías conservan alcances distintos.          |
| Regresión de bucles de heap por configuración | 100 puntos de estado completo y 30 inversas; se registran aparte de los puntos nuevos.                                                   |
| Núcleo normal                                 | 395 chequeos aprobados.                                                                                                                  |
| Entrega                                       | Seis programas nativos en el bloque 05; el perfil de bucles CPU rechaza exportación QASM plana.                                          |

Las fórmulas de referencia aplican la semántica C a valores de ambas amplitudes y sus fases.
Conservan las versiones físicas anteriores sin tomar valores de certificados del motor. Cada punto
se compara también con la evolución independiente de las puertas registradas por el runtime real. Se
comprueban todas las amplitudes presentes y los ceros privados esperados, y luego se invierte el
flujo completo de puertas. Estos perfiles finitos no son una prueba de estados cuánticos
arbitrarios.

Las construcciones y campañas finales comparten **216 entradas congeladas**, identificadas en
[summary.json](summary.json). `evidence.tar.gz` contiene fuentes, casos, estados, puertas, comandos,
códigos de salida, logs y orquestación. `tool-sha256.json` identifica los componentes construidos.
ASan/UBSan cubren plugin, driver, motor, runtime, adaptador y programas. GCC host mantiene la
exclusión existente de atribución de fugas de su proceso; los programas mantienen detección de
fugas.

`earlier-attempts.tar.gz` conserva el predecesor congelado de 214 entradas y los primeros intentos.
Las reproducciones iniciales mostraron los rechazos de bucles. Dos fixtures posteriores intentaron
observar fuentes antes de sus consumidores y fueron rechazados por el contrato; los finales observan
en el orden correcto. También quedan registrados dos fallos de invocación de la construcción, un
wrapper omitido al enlazar el adaptador y una expectativa de texto QASM que usaba el mensaje
específico del heap en lugar del rechazo real de control CPU. Los intentos repetidos no incrementan
los recuentos finales. Los prototipos anteriores al congelado tienen procedencia de fuentes
incompleta y no califican la entrega.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
bash scripts/run-block.sh 05
CKET_SIMULATOR=sparse build/blocks/05/fixed_buffers 3
python3 scripts/validate-fixed-memory-loops.py --output build/fixed-loop-proof --levels O0 O2
```

La historia sigue limitada a 128 cables físicos, con simulación densa de hasta 24 y modo disperso
acotado. Permanecen pendientes tamaños y direcciones CPU generales en la ruta fija, flujos y vidas
de propiedad arbitrarios, más ABI/punteros, cadenas sobre heap y flotantes cuánticos. El inventario
conserva 762 entradas pendientes, 94 parciales y 368 de metadatos, ninguna completa. La revisión
publicada de papers mantiene sus propias 210 entradas y requiere integrar los cambios nuevos.
Ciberseguridad sigue clásica. Esta entrega no completa C cotidiano ni C11, y no demuestra QPU física
ni aceleración.
