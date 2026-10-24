# Historial cuántico de bucles CPU y funciones

Bloque cerrado el **24 de octubre de 2026**. Los nombres C pueden recibir versiones sucesivas en
bucles CPU sin sobrescribir información cuántica necesaria para invertirlas. Se integran resultados
de incremento/asignación del heap, acumulación y copias heap→buffer fijo. Las funciones pueden
copiar a su parámetro de buffer dentro de `while(n--)`; el contador por valor pertenece al callee.
El [contrato](../../c11-runtime-history.md) y el
[ejemplo](../../../examples/blocks/05/heap_iterations.c) describen el alcance usable.

`qassign` calcula antes de trasladar la identidad del dueño anterior a la historia. `qslot` reserva
un dueño limpio para cada captura ejecutada. Las dependencias y épocas de las inversas guardadas
siguen esas identidades. Trasladar un dueño no añade puertas; calcularlo usa puertas del motor. Los
originales desconocidos siguen en el estado conjunto. No hay mediciones ni resets implícitos, ni
copias independientes de un estado desconocido.

| Comprobación                                          | Resultado y alcance                                                                                                                                 |
| ----------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| Estados complejos completos                           | 300 puntos: 200 normales (`O0`, `O2`) y 100 ASan/UBSan (`O1`), ambas políticas de síntesis.                                                         |
| Inversión independiente del flujo completo de puertas | 90 recorridos hasta el estado inicial.                                                                                                              |
| Perfiles finitos                                      | Escalares, transferencias, postfix/acumulación, ramas CPU y parámetros de función; 0, 1 y 3 iteraciones.                                            |
| Contratos del motor por configuración                 | 10 esquemas inválidos rechazados antes de mutar, 4 transportes de inversas con estados completos y un límite explícito de 128 cables: 15 contratos. |
| Contextos nativos adicionales                         | 32 ejecuciones: `do/while`, bucles anidados, ramas, argumentos de función con incremento y el ejemplo con 0, 1, 3 y 4 iteraciones.                  |
| Regresión de transferencias por configuración         | 56 puntos, 14 inversas, 6 programas válidos y 6 rechazos.                                                                                           |
| Regresión del heap por configuración                  | 58 puntos, 16 programas válidos y 16 rechazos.                                                                                                      |
| Otras regresiones normales                            | 395 chequeos del núcleo y 2332 del historial.                                                                                                       |
| Entrega usable                                        | Runner 05: cinco ejecutables nativos y cero exportaciones QASM de este perfil.                                                                      |

Los puntos comparan el estado conjunto complejo con fórmulas independientes de valores y fases, y
con un intérprete independiente de las puertas emitidas. Las fórmulas conservan los bits físicos de
versiones anteriores, sin tomar valores de certificados del motor. También se comprueba que una
transferencia vacía conserve los cables del destino. Las observaciones nativas adicionales tienen un
alcance distinto de las comparaciones de estado completo y no se suman a sus puntos.

Las dos construcciones y sus campañas finales usan las mismas **214 entradas congeladas**,
identificadas en [summary.json](summary.json). `evidence.tar.gz` contiene fuentes, casos generados,
comandos, códigos de salida, estados, puertas y logs. `tool-sha256.json` identifica los componentes
construidos. ASan/UBSan instrumentan plugin, driver, motor, runtime, adaptador y programas; se
comprueban también sus símbolos. GCC host conserva la exclusión existente de atribución de fugas del
proceso host; los programas generados mantienen la detección de fugas.

`earlier-attempts.tar.gz` conserva las fuentes congeladas y resultados de v1/v2/v3. Hubo un límite
de cables evitable antes de optimizar la acumulación, una expectativa negativa obsoleta, una campaña
sanitizada iniciada antes de terminar la construcción (sin ejecutar programas) y un fallo interno de
GCC al usar un parámetro de buffer. La versión final v4 corrige esos problemas. Los registros de
desarrollo anteriores al congelado tienen procedencia de fuentes incompleta y no certifican la
entrega. Los resultados repetidos de los intentos anteriores no aumentan los recuentos finales.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
bash scripts/run-block.sh 05
CKET_SIMULATOR=sparse build/blocks/05/heap_iterations 3
build/validate_runtime_versions
python3 scripts/validate-runtime-versions.py --output build/runtime-history-proof --levels O0 O2
```

La historia consume qubits: se conserva el límite físico de 128 cables, el límite denso de 24 y el
modo disperso acotado. Quedan pendientes declaraciones cuánticas dentro de bucles/control,
condiciones de bucle cuánticas generales, retornos tempranos y propiedad en flujos arbitrarios,
escrituras indexadas generales, más bucles de memoria fija, cadenas sobre heap, ABI indirecta y
flotantes cuánticos. El inventario C11 mantiene 762 entradas pendientes, 94 parciales y 368 de solo
metadatos, ninguna completa. Ciberseguridad sigue clásica. Esta campaña no demuestra C cotidiano
completo, ejecución en QPU ni aceleración. La revisión publicada de los papers conserva su propio
predecesor de 210 entradas; integrar estos nuevos cambios en ella sigue pendiente.
