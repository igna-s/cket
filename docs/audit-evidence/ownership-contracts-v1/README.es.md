# Propiedad, copia y consumidores de buffers

Esta entrega implementa lecturas puras con índice CPU, acumulación/resta modular de expresiones
cuánticas independientes y recorridos de buffers fijos con resultados devueltos por funciones. El
usuario solicitó validar ahora los contratos de propiedad y copia. La ciberseguridad permanece
clásica y no se modifica el aislamiento de Windows/WSL.

## Resultado y alcance

La campaña identifica **195 entradas de fuente** y mantiene separados sus resultados:

| Comprobación                                          | Resultado                                                     |
| ----------------------------------------------------- | ------------------------------------------------------------- |
| Estados complejos conjuntos completos                 | 250 puntos normales y 50 con ASan/UBSan.                      |
| Inversión independiente del flujo completo de puertas | 140 casos normales y 28 instrumentados.                       |
| Rechazos de propiedad                                 | 20 normales y 4 instrumentados.                               |
| Rechazos de dominios indefinidos o fuera de rango     | 40 normales y 8 instrumentados.                               |
| Conversiones/aritmética válida en límites elegidos    | 35 programas normales y 7 instrumentados.                     |
| Ejemplos recientes                                    | 7 ejecutables independientes terminan correctamente.          |
| Runner del bloque 03                                  | 4 ejecutables; 2 exportaciones QASM con informes de recursos. |
| Runner del bloque 04                                  | 1 ejecutable y 1 exportación QASM con informe de recursos.    |

La configuración normal recorre `-O0`, `-O1`, `-O2`, `-O3` y `-Os`, con ambas políticas de síntesis.
La instrumentada usa `-O1`. ASan/UBSan instrumentan plugin, driver, motor, runtime, programas
generados y observador. El validador comprueba los símbolos de instrumentación. GCC anfitrión no
está instrumentado; el wrapper existente desactiva la atribución de fugas solo en sus procesos,
manteniendo ASan/UBSan activos en el plugin. Los programas ejecutados conservan detección de fugas.

Las regresiones normales incluyen 395 comprobaciones nativas, 3505 de expresiones, 271 de
actualizaciones, 414 de copia de memoria, 2332 de historia y 1270 de operandos controlados. El
núcleo nativo repite sus 395 comprobaciones en la configuración instrumentada. Los números de cada
suite tienen sus propios alcances; no se suman como estados cuánticos independientes.

## Cómo se comprueba el contrato físico

El [validador](../../../scripts/validate-ownership.py) observa el runtime real mediante
[`ownership_trace.c`](../../../tests/ownership_trace.c). No reemplaza sus operaciones ni sus
capturas CPU. Interpreta de forma independiente las puertas y compara las amplitudes complejas del
estado conjunto completo, con tolerancia absoluta `1e-10` y sin ignorar fases.

Copias, movimientos y consumidores puros tienen además fórmulas explícitas para el estado conjunto,
incluidos auxiliares en cero. Las escrituras con historia y las conversiones comparan toda la
evolución de puertas y comprueban los valores públicos y sus fases; no se prescribe de forma
independiente la codificación privada de la historia. La limpieza de las conversiones debe dejar
cero en todos los cables privados. La inversa del flujo completo se comprueba aparte de las inversas
que emite el compilador en los puntos observados.

Los 14 perfiles incluyen movimiento, tres registros correlacionados por copia y su inversa,
consumidores puros, sobrescritura con historia desconocida, índices CPU capturados antes de su
mutación, índices cuánticos, recorridos con suma/resta, punteros con offset, palabras de 16 bits,
escritura coherente, escritura por búsqueda guardada y `atoi`/`atol`/`atoll`. Antes de cada punto no
puede haber mediciones ni resets. `atol` y `atoll` se consumen como 8 bits, sin certificar sus
límites completos de 64 bits.

Los casos negativos exigen diagnósticos semánticos concretos. Un proceso terminado por señal, un
fallo de infraestructura o la falta de un ejecutable no cuentan como rechazo correcto. Los casos
válidos de frontera conservan los casts explícitos, el módulo sin signo y las ramas inactivas.

## Errores y ajustes conservados

La primera ejecución aceptó incorrectamente `quant uint8_t x = 1; quant int y = INT_MAX; x += y`.
GCC había distribuido el estrechamiento sobre la suma y ocultado su dominio firmado de 32 bits. La
corrección restaura las promociones conservadas en el árbol y comprueba la operación completa antes
de convertir el resultado. Conserva los casts explícitos.

La regresión también encontró que la nueva ruta independiente rechazaba `x = x*2 + x`, antes
admitido. Esa expresión vuelve a la ruta reversible general que ya la implementaba. Las sumas
independientes conservan la nueva ruta con limpieza de auxiliares.

Dos pruebas antiguas todavía esperaban rechazar toda observación con índice cuántico. Se agregan
comprobaciones de la correlación observada y se mantienen rechazos para `reset` de una selección
calculada. Otra prueba de historia invalidaba la definición, pero conservaba el nuevo certificado
independiente del valor físico. Ahora invalida ambos cuando pretende probar la ausencia de una
demostración; sigue comprobando rechazo antes de mutar estado o puertas.

`earlier-attempts.tar.gz` conserva la fuente previa al arreglo del desbordamiento, el caso que fue
aceptado, sus salidas reales, intentos intermedios y fallos de las regresiones. Esos fallos no se
reetiquetan como aprobados ni se mezclan con los resultados finales.

## Reproducción y pendientes

En Linux/WSL con el toolchain GCC 11 declarado y sus dependencias:

```bash
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
python3 scripts/validate-ownership.py --compiler-root . \
    --output build/ownership-reproduced --levels O0 O1 O2 O3 Os
CC=gcc-11 bash scripts/run-block.sh 03
CC=gcc-11 bash scripts/run-block.sh 04
```

El directorio de salida del validador debe ser nuevo. `--sanitized` requiere una construcción con
todos los componentes instrumentados y el wrapper GCC correspondiente; no basta con instrumentar el
observador. El archivo conserva comandos de construcción, herramientas de orquestación, fuentes,
logs, códigos de salida, circuitos exportados y manifiestos SHA-256.

Los bucles CPU de los dos recorridos producen ejecutables nativos. No se exportan como circuitos
QASM estáticos. Sus resultados están separados de las dos exportaciones estáticas del bloque 03.

Siguen pendientes vectores dinámicos, direcciones y vidas generales, más combinaciones de flujo,
operadores y recursos, `float`/`double` cuánticos, integración amplia y una nueva auditoría de los
papers. Esta campaña no prueba ausencia de bugs, clonación independiente, todos los estados de
entrada, todo el [perfil cotidiano](../../perfil-c-cotidiano.es.md), toda C11 ni ejecución en QPU.

[`summary.json`](summary.json) identifica fuentes y hashes de los archivos. La identidad se
comprueba también contra el archivo del commit real antes del push. Los informes históricos de los
papers mantienen su revisión y no se regeneran como consecuencia de estas pruebas dirigidas.
