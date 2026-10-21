# Entregas cuánticas utilizables

El objetivo completo sigue abierto. A partir de ahora se entrega por bloques que se puedan compilar,
ejecutar y revisar por separado. Cada bloque incorpora código o una capacidad utilizable, ejemplos,
instrucciones y pruebas dirigidas. La auditoría amplia y los papers conservan su propia evidencia y
no frenan todas las entregas pequeñas.

**“10%, 20%…” significa 1, 2… de los diez bloques de este plan. No mide el porcentaje de C11
implementado ni el esfuerzo restante.** Los bloques tienen tamaños distintos. La
[matriz de cobertura](c-coverage.md) sigue siendo la referencia para cada construcción.

## Bloque 01: base entera cuántica utilizable

Esta entrega reúne ocho programas pequeños para el simulador incluido. También añade división
reversible por constantes de magnitud 2 a 16 y evita construir un cociente entero completo cuando
solo se necesita su verdad booleana. Se conservan los límites de tipos, dominio y recursos del
[contrato de división](c11-division.md).

Desde la raíz de un checkout Linux/WSL con GCC 11 y sus dependencias instaladas:

```bash
export CC=gcc-11
export CXX=g++-11
bash scripts/build.sh
bash scripts/run-block.sh 01
```

El comando compila y ejecuta los ejemplos, y deja sus ejecutables Linux, circuitos OpenQASM 3 e
informes de recursos en `build/blocks/01/`. Cualquier error interrumpe la entrega. Para volver a
usar un programa ya compilado:

```bash
build/blocks/01/quantum_buckets
cat build/blocks/01/quantum_buckets.resources.txt
```

| Programa              | Capacidad que puedes usar                                     | Resultado esperado                       |
| --------------------- | ------------------------------------------------------------- | ---------------------------------------- |
| `hello`               | Preparación y suma reversible de un byte.                     | `12`                                     |
| `entanglement`        | Dos booleanos entrelazados mediante control coherente.        | `0 0` o `1 1`                            |
| `fourier`             | Fourier y su inversa sobre un registro.                       | `7`                                      |
| `phase_estimation`    | Fases controladas e interferencia mediante Fourier inversa.   | `3`                                      |
| `functions`           | Composición de funciones con datos cuánticos.                 | `4`                                      |
| `modular_arithmetic`  | Suma, resta y XOR de bytes sin signo.                         | `add=14 subtract=240 xor=255`            |
| `coherent_accumulate` | Acumulación bajo un booleano sin medirlo dentro de las ramas. | `flag=0 total=30` o `flag=1 total=15`    |
| `quantum_buckets`     | Dividir una superposición en grupos antes de medir.           | `value=V bucket=V/3`, con V entre 0 y 15 |

En `quantum_buckets`, `value` empieza en superposición. El circuito calcula `value / 3` conservando
su correlación con el valor original; las dos llamadas a `measure` son explícitas. El programa
comprueba que ambas observaciones coincidan con la relación indicada.

El paquete usa registros pequeños: la simulación densa nativa tiene un límite de **24 qubits** y la
representación de circuitos, de **128**. Un `quant int` normal ocupa 32 bits; poder exportarlo no
implica poder ejecutarlo en este simulador. Tampoco hay una ejecución demostrada en QPU física o una
aceleración demostrada frente a CPU. Estos ejemplos no certifican todas las combinaciones de sus
operaciones.

**Cierre comprobado:** reconstrucción Linux nueva, ocho ejecuciones con sus ocho exportaciones y
1509 comprobaciones de los 37 ejemplos en cinco niveles de optimización. El ejemplo de grupos
observó `value=10 bucket=3` en la ejecución del paquete; otras muestras válidas pueden variar. Su
informe estático registra un pico de 16 qubits. La
[evidencia del bloque](audit-evidence/common-division-v3/README.es.md) separa esta construcción de
las pruebas normales y con sanitizadores de la división.

## Avance utilizable del bloque 02

Ya está disponible `bash scripts/run-block.sh 02`, con cuatro programas de enteros anchos en el
[motor disperso](simulacion-dispersa.es.md): división con `int` de 32 bits y un cálculo con `size_t`
de 64 bits, además de desplazamiento variable de 32 bits con un contador cuántico. El runner genera
los ejecutables, circuitos e informes de recursos. El soporte ocupado tiene un presupuesto
explícito; no se promete simular cualquier estado de 128 qubits.

El cuarto programa, `shift_predicate`, usa un desplazamiento de 64 bits como booleano y después de
una conversión a byte. Ya ejecuta con 85 qubits, sin construir el resultado entero completo para
preguntar si es cero. Se corrigió además una negación lógica incorrecta de la ruta aritmética.

Este avance mantiene el bloque 02 abierto: faltan combinaciones y contextos enteros generales.
Permite usar la nueva capacidad mientras se sigue trabajando en el alcance completo.

## Avance del bloque 06: cadenas

`bash scripts/run-block.sh 06` reúne `string_spans`, `string_search`, `search_positions`,
`saved_search`, `string_copy`, `string_concat`, `controlled_concat` y `byte_copy`: longitudes con
`strspn` y `strcspn`, y comprobaciones de existencia con `memchr`, `strchr`, `strrchr`, `strpbrk` y
`strstr`. Se añaden diferencias de posiciones y comparaciones con direcciones del mismo objeto,
además de préstamos locales inmutables, alias y lecturas de caracteres mediante desreferencia o
índices constantes. Los datos permanecen cuánticos hasta las mediciones explícitas de los ejemplos.
Se usa el motor disperso y se generan los ejecutables, circuitos e informes de recursos.

`string_copy` añade `strcpy` y `strncpy`, una fuente en superposición, cola preservada, padding y
escritura parcial de un entero. El [contrato de copia](c11-string-copy.md) describe el alcance.

`string_concat` añade `strcat` y `strncat`: longitud inicial en superposición, captura de longitudes
antes de escribir, cola preservada y append desde una fuente sin NUL. El
[contrato](c11-string-concat.md) describe los límites.

`controlled_concat` añade concatenación bajo un booleano cuántico y un ejemplo de fase e
interferencia mediante Fourier. Su resultado esperado es `1`; la calificación completa de estados e
inversas sigue aplazada.

`byte_copy` amplía `memcpy` y `memmove`: sobrescrituras con historia, bytes parciales, tamaños
enteros diferentes y fuentes CPU conocidas. Compara un movimiento solapado y las escrituras
parciales con libc. El [contrato de memoria](c11-memcopy.md) distingue la ruta rápida de elementos
completos y la nueva ruta de bytes. La calificación exhaustiva sigue pendiente.

El [contrato](c11-string-searches.md) limita las vistas y los consumidores de punteros. La matriz
completa de estados, inversas y sanitizadores queda aplazada al cierre del bloque para priorizar
implementación. El bloque 06 sigue abierto y no cuenta como otra entrega completa.

## Orden de las siguientes entregas

| Bloque | Entrega utilizable que se debe cerrar                                                            | Estado                                           |
| ------ | ------------------------------------------------------------------------------------------------ | ------------------------------------------------ |
| 01     | Base anterior, ejemplos ejecutables y exportación.                                               | Integrada; comandos y límites documentados.      |
| 02     | Tipos enteros comunes y consumidores: ampliar operadores, conversiones, condiciones y recursos.  | Avance usable de int32/size_t64; bloque abierto. |
| 03     | Funciones y flujo de control habituales con contratos de vida e inversa.                         | Parcial.                                         |
| 04     | Arrays y estructuras de tamaño fijo en programas compuestos.                                     | Parcial.                                         |
| 05     | Punteros y operaciones de memoria dentro de rangos comprobados.                                  | Adaptadores parciales.                           |
| 06     | Cadenas y clasificación/conversión de caracteres.                                                | Adaptadores parciales.                           |
| 07     | Resto de interfaces enteras habituales de la biblioteca C.                                       | Parcial o pendiente según interfaz.              |
| 08     | Aritmética `float`, `double` y compleja con semántica declarada.                                 | Pendiente.                                       |
| 09     | Construcciones e interfaces restantes del inventario C11, con contratos explícitos por contexto. | Pendiente en gran parte.                         |
| 10     | Integración final, compatibilidad documentada y cierre de afirmaciones del repo y los papers.    | Pendiente.                                       |

Las capacidades parciales de un bloque futuro siguen disponibles; solo se lo contará como entregado
cuando tenga un alcance concreto y comprobado. En cada avance se prioriza implementar y publicar ese
alcance antes de abrir nuevas matrices de auditoría. Se ejecutan las pruebas necesarias del cambio y
se amplían cuando aparece un fallo o una incertidumbre concreta.

La ciberseguridad, criptografía y autenticación permanecen clásicas. Este plan tampoco requiere
cambiar el aislamiento o la configuración de seguridad de Windows/WSL.
