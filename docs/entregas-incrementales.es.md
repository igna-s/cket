# Entregas cuánticas utilizables

**Alcance actualizado el 23 de octubre de 2026:** cerrar el
[núcleo de C cotidiano](perfil-c-cotidiano.es.md), con funciones, arrays, punteros, vectores y las
operaciones frecuentes que necesitan. El objetivo sigue abierto y se entrega por bloques que se
puedan compilar, ejecutar y revisar por separado. La auditoría y los papers conservan su propia
evidencia. El usuario abrió ahora la validación de propiedad y copia: la
[campaña dirigida](audit-evidence/ownership-contracts-v1/README.es.md) añade ejecuciones, estados,
fases e inversas de las novedades. Sigue pendiente cerrar la integración completa del perfil.

El plan anterior de diez bloques se conserva abajo como referencia ampliada. **“10%, 20%…” no mide
el porcentaje de C11 implementado ni el esfuerzo restante.** El alcance vigente se cierra por
capacidades y programas integrados del perfil, sin exigir todas las interfaces poco habituales. La
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

## Implementación del bloque 03: funciones con buffers

`bash scripts/run-block.sh 03` reúne `buffer_functions` y `buffer_words`: funciones encadenadas que
leen un buffer con índice cuántico, préstamos de solo lectura, alias con desplazamiento constante y
escrituras con historia. El índice se recupera antes de multiplicarlo por el tamaño del elemento. El
[contrato](c11-buffer-functions.md) distingue estos casos de punteros y memoria dinámica generales.
Se añaden `cpu_vector_scan` y `cpu_vector_return`: recorridos con índices CPU sobre buffers fijos,
suma/resta y paridad en dueños estables, y resultados escalares devueltos por funciones. Los cuatro
ejemplos ejecutan y la campaña de propiedad comprueba estados e inversas de casos elegidos. El
runner exporta los dos ejemplos estáticos; los dos recorridos CPU generan ejecutables nativos,
porque esos bucles no se exportan a QASM. La integración general sigue pendiente; el bloque 03 sigue
abierto.

## Implementación del bloque 04: índices cuánticos

`bash scripts/run-block.sh 04` reúne el nuevo fuente `quantum_index`. Se implementan lecturas y
escrituras sobre arrays escalares comprobados mediante un índice cuántico, con resultados de
asignación e incremento. Su [contrato](c11-coherent-index.md) conserva los límites y los pendientes.
Su ejemplo ya ejecuta y hay pruebas finitas de estados, fases e inversas en la campaña de propiedad.
La cobertura exhaustiva de consumidores y recursos sigue pendiente. El bloque 04 sigue abierto.

## Avance utilizable del bloque 05: heap y vectores dinámicos

`bash scripts/run-block.sh 05` compila y ejecuta `dynamic_heap` y `coherent_heap`. En el primero, la
capacidad inicial es un dato CPU recibido por `argv`:
`CKET_SIMULATOR=sparse build/blocks/05/dynamic_heap 4`. El programa reserva con `calloc`, llena,
modifica bajo un booleano cuántico, crece con `realloc`, recorre mediante una función, mide
explícitamente y libera. Compara la suma con los elementos observados y una fórmula C ordinaria.
Este avance genera un ejecutable nativo; el control CPU y el heap no se exportan como un circuito
QASM estático.

El [contrato de heap](c11-heap.md) explica la identidad de los registros, los préstamos, los
resultados derivados que siguen vivos al redimensionar/liberar y los límites de recursos. No hay
mediciones ni reset implícitos. Se añade `coherent_heap`, con incremento mediante un índice cuántico
y captura de su valor anterior. También se implementan resultados de asignación e incremento en
consumidores programados fuera de bucles CPU. Faltan esos resultados dentro de bucles CPU, pruebas
de direcciones disjuntas y adaptadores de bytes/cadenas. El bloque 05 sigue abierto.

## Referencia del plan anterior de diez bloques

Los números identifican las entregas existentes. Las filas que cubren todo el inventario, complejos
y otras interfaces poco habituales son una ampliación posible; no condicionan el cierre del perfil
vigente. La prioridad de implementación está en el [alcance actual](perfil-c-cotidiano.es.md).

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
alcance antes de abrir nuevas matrices de auditoría. La validación actual de propiedad y copia fue
solicitada después por el usuario y conserva su alcance y resultados por separado.

La ciberseguridad, criptografía y autenticación permanecen clásicas. Este plan tampoco requiere
cambiar el aislamiento o la configuración de seguridad de Windows/WSL.
