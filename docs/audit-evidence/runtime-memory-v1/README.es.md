# Memoria fija con longitudes CPU

Entrega del **24 de octubre de 2026**. `memcpy`, `memmove` y `memset` aceptan longitudes CPU puras
sobre vistas enteras comprobadas, hasta 64 bytes. El
[ejemplo](../../../examples/blocks/05/buffer_lengths.c) recibe la longitud por `argv`, la pasa a una
función y combina las tres operaciones. El [contrato](../../c11-runtime-memory.md) detalla las
direcciones, vidas, recursos y consumidores pendientes.

| Comprobación                  | Resultado y alcance                                                                                                                                         |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Estados complejos conjuntos   | 900 puntos: 600 normales (`O0`, `O2`) y 300 ASan/UBSan (`O1`), ambas políticas de síntesis.                                                                 |
| Inversión completa de puertas | 270 recorridos hasta el estado inicial.                                                                                                                     |
| Perfiles                      | Copia, movimientos en ambos sentidos, relleno que lee su destino y bytes parciales; longitudes 0/1/2 e iteraciones 0/1/3.                                   |
| Fronteras mantenidas          | 9 ejecuciones con observación explícita del retorno y 12 rechazos de lectura/escape/escritura sin contrato.                                                 |
| Preflight por configuración   | 23 contratos de esquema, límites, solapamiento, tipos vinculados y controles inactivos antes de mutar.                                                      |
| Contextos adicionales         | 60 ejecuciones nativas, 8 rechazos de rango/solapamiento con estado intacto y sin lecturas CPU habilitadas, 8 rechazos de frontera y 6 identidades exactas. |
| Regresiones por configuración | 414 chequeos de copia y 940 de relleno; dos matrices anteriores de bucles, cada una con 100 puntos y 30 inversas.                                           |
| Núcleo normal                 | 395 chequeos aprobados.                                                                                                                                     |
| Entrega                       | Siete ejecutables nativos en el runner 05.                                                                                                                  |

Las fórmulas independientes conservan las versiones físicas anteriores y comparan todas las
amplitudes con el estado real y con evolución independiente de las puertas. No toman valores de
certificados del motor. Los puntos son perfiles finitos repetidos; las otras categorías no se suman
como cobertura de estados ni de interfaces.

[summary.json](summary.json) identifica las mismas **221 entradas congeladas** usadas por las
construcciones y campañas finales. `evidence.tar.gz` contiene fuentes, casos, estados, puertas,
comandos, códigos de salida y logs. `tool-sha256.json` identifica los componentes y el ejemplo.
ASan/UBSan cubren plugin, driver, motor, runtime y programas; el host GCC conserva su exclusión
existente de atribución de fugas, y los programas mantienen detección de fugas.

Se corrigen una unión anidada que invalidaba un dueño del otro brazo y una lectura del retorno que
obtenía un placeholder CPU: en la reproducción dio `0` frente al valor cuántico `3`. La observación
explícita ahora obtiene el registro real. Se rechazan lecturas CPU sin `measure`, escapes a
llamadas/aggregados CPU y escrituras directas sin préstamo local comprobado.

`earlier-attempts.tar.gz` conserva las versiones congeladas v1/v2 y sus resultados, los fallos
reproducidos y los prototipos. Estos últimos tienen procedencia de fuentes incompleta y no califican
la entrega. Un helper esperaba otro mensaje para un rechazo QASM correcto; sus ejecuciones nativas
se conservaron y se completaron las cuatro comprobaciones restantes mediante una revisión
registrada. Los intentos repetidos no aumentan los recuentos finales.

Persisten los límites de 128 cables, simulación densa de 24 y soporte disperso acotado. Faltan
direcciones/ABI generales, más flujos de propiedad, cadenas sobre heap y flotantes. El inventario
sigue con 762 entradas pendientes, 94 parciales y 368 de metadatos. La nota técnica nueva tiene su
auditoría editorial propia; los manuscritos históricos conservan sus revisiones. No se demuestra C
cotidiano completo, ejecución en QPU ni aceleración. Ciberseguridad sigue clásica.
