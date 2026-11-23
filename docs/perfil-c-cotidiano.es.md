# Alcance actual: C cotidiano funcional

**Acordado el 23 de octubre de 2026.** El objetivo actual es poder escribir programas completos con
las herramientas habituales de C: funciones, tipos comunes, control, arrays, punteros y vectores. La
compatibilidad debe funcionar en conjunto dentro de un perfil explícito. La idea de «el 50% que usás
siempre» expresa esta prioridad; no es una cifra de cobertura medida.

La línea base publicada archivada en CI corresponde al commit `5c5a965` (corrida `42507019584`), en
[la cápsula `full-ci-everyday-profile-v1`](audit-evidence/full-ci-everyday-profile-v1/README.es.md).
Sobre esa base, la rama activa integra y verifica en local (normal + ASan/UBSan) las olas Wave 1A

- Wave 1B + Wave 2 + Wave 3: `1024` wires físicos en `qc_mask`, heaps de hasta `1024` elementos,
  divisor de hasta `128` bits en el motor, `float`/`double` IEEE-754 completo con `<math.h>`
  reversible (`fabs`/`copysign`/`signbit`/`fmin`/`fmax`/`fdim`/`trunc`/`floor`/`ceil`/`round`/`sqrt`
  y predicados
  `isnan`/`isinf`/`isfinite`/`isnormal`/`isgreater*`/`isless*`/`islessgreater`/`isunordered`),
  optimizador de escalado por potencias de 2, puente IR entero-flotante y `++`/`--` en
  `float`/`double` (`68469` comprobaciones en `validate_float`), las capacidades del perfil
  `A1`–`A12` (incluidos `const` y globales `static`, structs con
  padding/bitfields/uniones/punteros/VLA acotado, recursión acotada, variádicas, punteros a función
  `quant` indirectos, compilación multi-TU, `while` acotado, `goto` hacia adelante limpio y
  adaptadores ampliados de `stdlib`), los `7` ejemplos integrados nuevos (incluido
  `everyday_math_geometry`, `76` ejemplos y `3667` comprobaciones en `test_examples`) y la ejecución
  íntegra local de `scripts/verify.sh` (`scripts/sanitize-all.sh`), `scripts/run-examples.sh --all`,
  `scripts/sanitize.sh` y `validate-everyday-profile.py` (`O0`/`O2` normal y `O1` sanitizado).
  Restan fuera de este perfil las funciones trascendentales de `<math.h>`, C11 completo,
  `long double`/`_Complex`, QPU física y CFG/ABI/vidas arbitrarias.

El inventario de todas las interfaces C11 se conserva como referencia técnica y posible ampliación.
Completar cada una de sus entradas deja de ser condición para cerrar este alcance. Una declaración
aceptada, un diagnóstico correcto o un ejemplo que solo compila tampoco completan una capacidad.

## Qué debe poder usarse

| Parte del núcleo         | Resultado que se busca                                                                                            | Estado actual y trabajo pendiente                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Tipos y operadores       | `bool`, caracteres, enteros con y sin signo, aliases, `size_t`, conversiones y operadores habituales.             | Hay representación y síntesis entera hasta 64 bits con divisor de hasta 128 bits (`QC_MAX_PHYSICAL = 1024`), división/módulo de 64 bits (`A1`) y cierre de recursos en el perfil `A1`–`A12`; fuera de él siguen existiendo límites de presupuesto y combinaciones arbitrarias.                                                                                                                                                                                                                                                                                                            |
| Funciones                | Parámetros, llamadas entre funciones, resultados, locales y funciones que procesan buffers.                       | Composición en la misma unidad, compilación/enlace multi-TU (`A8`), punteros a función `quant` indirectos (`A10`), recursión acotada y variádicas (`A11`), préstamos `const`/mutables, movimiento/asignación de `struct`, guardas y resultados propios. Faltan usos arbitrarios sin cota y ABI general fuera del perfil.                                                                                                                                                                                                                                                                  |
| Control                  | Condiciones, `switch`, `for`, `while`, `do while` y salidas habituales con efectos y limpieza correctos.          | Condiciones coherentes, `switch` estructurados (CPU y cuánticos con `case`, rangos, `default`, `break`, `continue`, `return`), bucles `for`/`while`/`do-while` acotados y `while` con guarda cuántica más `goto` hacia adelante sobre ámbito limpio (`cfg_while_q_and_forward_goto`). Quedan fuera `goto` hacia atrás/ciclos arbitrarios y bucles cuánticos sin cota estática.                                                                                                                                                                                                            |
| Arrays y structs básicos | Vectores, matrices, campos enteros, recorridos, lecturas y actualizaciones.                                       | Arrays, matrices multi-eje, structs con padding/bitfields/uniones pequeñas/campos puntero y VLA estáticamente acotado (`A4`), con lectura/escritura coherente e inversas verificadas en `validate-everyday-profile.py` y `everyday_matrix_struct.c`.                                                                                                                                                                                                                                                                                                                                      |
| Punteros y buffers       | Pasar un buffer y su longitud, desreferenciar, indexar, crear alias y escribir conservando procedencia y límites. | Préstamos comprobados encadenados (`A2`), parámetros `quant struct *`, reasignación de puntero con escritura CPU/byte-punning (`A5`), e índices cuánticos sobre escalado canónico. Faltan punteros/vidas arbitrarias fuera del contrato comprobado.                                                                                                                                                                                                                                                                                                                                       |
| Vectores dinámicos       | Reservar almacenamiento, mantener longitud/capacidad, recorrer, actualizar y liberar con una vida definida.       | `malloc`, `calloc`, `aligned_alloc`, `realloc` y `free` con capacidad CPU de hasta `1024` elementos (`QC_HEAP_MAX_ELEMENTS`), soporte `bool`/`struct` (`A6`) e historia por iteración (`everyday_dynamic_vector.c`). Faltan pruebas generales de direcciones disjuntas y ABI arbitraria.                                                                                                                                                                                                                                                                                                  |
| Biblioteca frecuente     | Copiar, mover y llenar memoria; manejar cadenas y conversiones habituales dentro de contratos comprobados.        | Adaptadores verificados para `memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strcat`, `strncat`, `memchr`, `strchr`, `strrchr`, `strpbrk`, `strstr`, `strspn`, `strcspn`, `atoi`, `atol`, `atoll`, `strtol`, `strtoll`, `strtoimax`, más las extensiones `strtoul`, `strtoull`, `strtoumax`, `strtok`, `strcoll`, `strxfrm`, `strerror`, `qsort`, `bsearch`, `snprintf`, `sscanf` y `uselocale` (`A12`, `everyday_buffers_strings.c`). El resto de libc sigue pendiente.                                                                    |
| Cálculo real habitual    | `float` y `double` cuánticos, operaciones y conversiones con semántica definida.                                  | Implementado de punta a punta para IEEE-754 binary32 (`float`) y binary64 (`double`) con `<math.h>` reversible (`sqrt`, `fmin`, `fmax`, `fdim`, `trunc`, `floor`, `ceil`, `round`, `copysign`, `signbit`, `fabs` y predicados no ordenados), optimizador de constantes/potencias de 2, puente IR entero-flotante y `++`/`--` en coma flotante: `validate_float` (`68469` comprobaciones), `examples/float_double.c`, `examples/everyday_math_geometry.c` y casos `b1_*`/`b2_*` en normal y ASan/UBSan. Trascendentales de `<math.h>`, `long double` y `_Complex` siguen fuera de alcance. |
| Entrada/salida           | Leer datos clásicos, preparar entradas, medir explícitamente y mostrar o guardar resultados.                      | Frontera CPU existente; debe componerse con los programas del perfil sin mediciones ocultas.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |

La ciberseguridad, criptografía y autenticación permanecen clásicas. El cierre de este perfil no
requiere completar caracteres anchos, complejos, atomics, threads ni todas las interfaces poco
habituales. Sus entradas pendientes siguen visibles en el inventario.

El bloque posterior a RC3 corrige la coherencia de bytes intactos en cuatro adaptadores fijos de
cadenas. La [nota](paper/string_writes_note.es.md) delimita los programas comprobados y los casos
que exceden los recursos. El [seguimiento de efectos seleccionados](paper/lazy_values_note.es.md)
corrige las referencias de historial y las lecturas antiguas pendientes durante efectos posteriores.
La [campaña posterior](audit-evidence/c-priority-integration-v1/README.es.md) completa 2332
comprobaciones de historial tanto en normal como con ASan/UBSan. La campaña histórica de 355 casos
dirigidos conserva su alcance original; posteriormente, la línea base `5c5a965` completó el CI
íntegro en
[la cápsula `full-ci-everyday-profile-v1`](audit-evidence/full-ci-everyday-profile-v1/README.es.md).

El avance del 24 de octubre añade `memcpy` y `memmove` entre heaps y `memset` sobre heaps, con
longitudes CPU de ejecución, vistas de bytes parciales y retornos de dirección clásicos. Se
conservan los originales para los movimientos solapados y las inversas. También se copian buffers
cuánticos fijos y heaps en ambas direcciones, y objetos CPU conocidos hacia heaps. El destino fijo
conserva sus qubits fuera de la longitud solicitada. Sus escrituras dentro de bucles CPU y los
resultados de incremento del heap añaden historia por iteración. Las cadenas sobre heap siguen
pendientes; el [contrato](c11-heap.md) detalla los contextos y recursos.

Las copias, movimientos solapados y rellenos entre buffers fijos también conservan versiones dentro
de bucles CPU. [fixed_buffers.c](../examples/blocks/05/fixed_buffers.c) combina esas operaciones en
una función con contador por valor. La
[evidencia finita](audit-evidence/fixed-memory-loops-v1/README.es.md) no completa las direcciones
generales, retornos ni flujos arbitrarios de propiedad. Los
[tamaños CPU de ejecución](c11-runtime-memory.md) ya seleccionan prefijos de buffers fijos
comprobados; `buffer_lengths.c` pasa esa longitud a una función y observa los resultados.

La [corrección de préstamos de lectura](c11-readonly-borrows.md) admite alias `const` en parámetros
escalares y de buffers fijos. Rechaza las escrituras por casts antes de perder el ámbito del
préstamo. Su evidencia de 239 entradas distingue los casos comprobados de las combinaciones
generales de alias y vidas que siguen pendientes.

La [ampliación de retornos CPU](c11-procedure-returns.md) implementa salidas anticipadas de
procedimientos `void` en bloques y bucles, con limpieza léxica y continuación del llamador. Sus
estados, fases e inversas tienen evidencia propia; posteriormente, `5c5a965` incorpora resultados
propios imperativos en funciones `quant`, mientras el CFG general sigue pendiente.

La [composición con un llamador coherente](paper/coherent_returns_note.es.md) ya admite esos
retornos cuando los predicados CPU son puros. No habilita salidas decididas por datos cuánticos,
efectos CPU observables, bucles coherentes ni vidas generales.

La ampliación del 28 de octubre habilita `switch` estructurados (clásicos CPU y coherentes cuánticos
con casos simples, rangos, casos apilados, `default`, `break`, `continue` a través del `switch` y
`return`), funciones `quant` de valor con cláusulas de guarda y múltiples `return`, y la composición
de `memcpy`, `memmove`, `memset` y parches parciales con bucles CPU, `break`, `continue` y `return;`
bajo un llamador coherente `if (control)`, verificando matrices densidad reducidas del byte intacto
e inversas completas. En noviembre de 2026 (`5c5a965`, verificado en la
[corrida de CI 42507019584](audit-evidence/full-ci-everyday-profile-v1/README.es.md)) se añaden
además punteros y parámetros `quant` a estructuras (`p->campo`, `p[i].campo`), movimiento,
asignación y `reset` de productos completos (`quant struct S b = a;`, `d = b;`, `reset(m)`),
operaciones de memoria/cadenas (`memcpy`, `memmove`, `memset`, `memcmp`, `strncmp`, `strncpy`,
`strncat`, `memchr`, `strlen`, etc.) sobre estructuras sin relleno, matrices multidimensionales y
longitudes de bytes `quant`, conversiones de cadena a entero en base arbitraria `2..36` (`strtol`,
`strtoll`, `strtoimax` con `endptr` nulo o `&end` local además de `atoi`, `atol`, `atoll`),
intervalos de preparación diádicos desplazados `superposition(a, a + 2^k - 1)` con certificado de
intervalo de soporte para divisiones y rangos, indexación coherente multi-eje con índices cuánticos
sobre matrices, arreglos de estructuras y punteros de búsqueda (`m[q_i][q_j]`, `t[q_i].arr[q_j]`,
`p[q_i]`, `*(p + q_i)`), inicializadores de arreglos de bytes anidados en estructuras/matrices,
bucles `for`, `while` y `do-while` estáticamente acotados con guarda cuántica pura, contadores
predeclarados y pasos constantes arbitrarios (`+= k`, `-= k`, `!=`), propagación nativa de `quant` a
través de `typedef`, y funciones `quant` con resultado propio imperativo (escalar o `struct`),
bucles CPU y salidas anticipadas. Véase también el
[índice maestro del código](indice-del-codigo.es.md).

## Orden de trabajo

1. Integrar los enteros y sus consumidores con funciones, arrays y structs básicos.
1. Completar el recorrido habitual de punteros y buffers, incluidos alias y resultados de funciones.
1. Incorporar almacenamiento y vida de vectores dinámicos, con límites de recursos declarados.
1. Completar las operaciones frecuentes de memoria/cadenas que necesiten esos programas y después el
   cálculo habitual con `float` y `double`.
1. Cerrar la validación integrada y ajustar documentación y afirmaciones de los papers al resultado.

Se entrega capacidad utilizable por bloques. El usuario abrió ahora la validación de propiedad,
copias, fases e inversas. La [campaña dirigida](audit-evidence/ownership-contracts-v1/README.es.md)
ejecuta siete ejemplos recientes y comprueba estados de funciones, buffers, índices, historia y
conversiones. La evidencia finita nueva no completa la integración general de las filas anteriores.

## Cómo se determina que funciona

El cierre debe incluir programas completos que combinen las capacidades, con una referencia C
ordinaria para los resultados definidos. Los casos de aceptación deben incluir:

- Funciones que reciben un vector y su longitud, recorren sus elementos, acumulan y devuelven un
  resultado; llamadas encadenadas con locales y resultados intermedios.
- Arrays y matrices con lecturas y escrituras, índices CPU y cuánticos dentro del contrato, structs
  básicos y valores producidos por asignaciones e incrementos.
- Buffers compartidos entre funciones, alias admitidos, copias y movimientos de memoria, cadenas y
  conversión de datos, conservando los valores que siguen vivos.
- Un vector con almacenamiento dinámico que se reserva, se usa y se libera según su contrato; un
  nombre de ejemplo o un array fijo no sustituye esta capacidad.
- Combinaciones de condiciones y bucles que ejerciten limpieza de temporales, historia y retornos;
  medición explícita en la frontera de salida.

Para datos cuánticos, comprobar solo números medidos no alcanza: también hacen falta casos de fase,
correlación e inversa, contrastes independientes apropiados y agotamiento de recursos declarado. La
validación se concentra en este perfil y sus combinaciones. Cada resultado debe identificar las
fuentes que realmente se ejecutaron.

«100% funcional» significa cumplir los contratos de este perfil y sus programas integrados. No
promete recursos ilimitados, toda la biblioteca C11 (`long double`, `_Complex`, `goto` hacia
atrás/sin cota, ABI/CFG arbitrario) ni ejecución demostrada en una QPU física. Sobre esta rama
activa, los contratos y las 6 pruebas integradas del perfil cotidiano ya pasan completos en local
(normal y ASan/UBSan), restando el cierre del flujo remoto en GitHub Actions y los alcances fuera
del perfil.

Los contratos precisos se conservan en la [matriz de capacidades](c-coverage.md), los
[bloques entregados](entregas-incrementales.es.md) y el
[estado general](estado-general-del-proyecto.es.md).

Los [locales CPU privados y bucles](paper/private_cpu_loops_note.es.md) admiten inicializadores
puros, escrituras dentro de la misma rama cuántica de declaración y condiciones de iteración CPU
puras. Se excluyen direcciones tomadas, almacenamiento estático, `volatile`, `_Atomic`, punteros y
agregados privados. Los [saltos CPU estructurados](paper/loop_jumps_note.es.md) histórico-originales
añadieron `break` y `continue` con la limpieza y los incrementos de C, dejando pendientes en aquella
nota las decisiones cuánticas de salto, `continue` a través de `switch`, efectos CPU anidados y
vidas/alias/ABI generales. Posteriormente (`5c5a965`), se habilitó `continue` a través de `switch`;
los saltos arbitrarios y las vidas/alias/ABI generales continúan pendientes. Las notas y cápsulas
anteriores mantienen sus propias identidades y alcances.
