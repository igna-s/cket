# Alcance actual: C cotidiano funcional

**Acordado el 23 de octubre de 2026.** El objetivo actual es poder escribir programas completos con
las herramientas habituales de C: funciones, tipos comunes, control, arrays, punteros y vectores. La
compatibilidad debe funcionar en conjunto dentro de un perfil explícito. La idea de «el 50% que usás
siempre» expresa esta prioridad; no es una cifra de cobertura medida.

El inventario de todas las interfaces C11 se conserva como referencia técnica y posible ampliación.
Completar cada una de sus entradas deja de ser condición para cerrar este alcance. Una declaración
aceptada, un diagnóstico correcto o un ejemplo que solo compila tampoco completan una capacidad.

## Qué debe poder usarse

| Parte del núcleo         | Resultado que se busca                                                                                            | Estado actual y trabajo pendiente                                                                                                                                                                                                                                                                                                                                                                                                               |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Tipos y operadores       | `bool`, caracteres, enteros con y sin signo, aliases, `size_t`, conversiones y operadores habituales.             | Hay representación y síntesis entera hasta 64 bits. Faltan combinaciones de contextos, división general y cierre de recursos; no están completos todos los usos de cada tipo.                                                                                                                                                                                                                                                                   |
| Funciones                | Parámetros, llamadas entre funciones, resultados, locales y funciones que procesan buffers.                       | Composición directa en la misma unidad y préstamos parciales. Faltan usos generales de punteros, retornos y vidas; la ABI entre unidades conserva restricciones.                                                                                                                                                                                                                                                                                |
| Control                  | Condiciones, `for`, `while`, `do while` y salidas habituales con efectos y limpieza correctos.                    | Hay condiciones coherentes, bucles acotados y control CPU. Faltan combinaciones de flujo e historia; un bucle con condición cuántica general sigue pendiente.                                                                                                                                                                                                                                                                                   |
| Arrays y structs básicos | Vectores, matrices, campos enteros, recorridos, lecturas y actualizaciones.                                       | Arrays y productos fijos implementados con límites. La nueva lectura/escritura con índice cuántico admite arrays escalares de 1 a 64 elementos con ejecuciones y evidencia finita de estados e inversas.                                                                                                                                                                                                                                        |
| Punteros y buffers       | Pasar un buffer y su longitud, desreferenciar, indexar, crear alias y escribir conservando procedencia y límites. | Hay préstamos comprobados con funciones encadenadas e índices cuánticos sobre escalado canónico; hay lecturas con índices CPU y recorridos con acumulación y retorno. Se añaden heaps nativos con tamaño CPU, alias y préstamos de funciones. Faltan más formas de direcciones/vidas e integración general.                                                                                                                                     |
| Vectores dinámicos       | Reservar almacenamiento, mantener longitud/capacidad, recorrer, actualizar y liberar con una vida definida.       | Implementación parcial de `malloc`, `calloc`, `realloc` y `free`, capacidad CPU, préstamos y recorridos. `dynamic_heap.c` usa almacenamiento que crece realmente. Se añaden escrituras con índice cuántico y resultados de asignación/incremento. Los resultados dentro de bucles CPU y las copias heap→fijo añaden historia por iteración. Faltan pruebas generales de direcciones disjuntas y memoria/ABI generales. [Contrato](c11-heap.md). |
| Biblioteca frecuente     | Copiar, mover y llenar memoria; manejar cadenas y conversiones habituales dentro de contratos comprobados.        | Existen adaptadores parciales. Las nuevas conversiones decimales, escrituras por búsqueda e indexación coherente tienen ejecuciones y pruebas finitas; sus dominios generales siguen pendientes.                                                                                                                                                                                                                                                |
| Cálculo real habitual    | `float` y `double` cuánticos, operaciones y conversiones con semántica definida.                                  | Pendiente. Los flotantes clásicos de GCC y los ángulos de las puertas no implementan esos tipos cuánticos. Se abordan después de integrar el núcleo entero, funciones y memoria.                                                                                                                                                                                                                                                                |
| Entrada/salida           | Leer datos clásicos, preparar entradas, medir explícitamente y mostrar o guardar resultados.                      | Frontera CPU existente; debe componerse con los programas del perfil sin mediciones ocultas.                                                                                                                                                                                                                                                                                                                                                    |

La ciberseguridad, criptografía y autenticación permanecen clásicas. El cierre de este perfil no
requiere completar caracteres anchos, complejos, atomics, threads ni todas las interfaces poco
habituales. Sus entradas pendientes siguen visibles en el inventario.

El bloque posterior a RC3 corrige la coherencia de bytes intactos en cuatro adaptadores fijos de
cadenas. La [nota](paper/string_writes_note.es.md) delimita los programas comprobados y los casos
que exceden los recursos. El [seguimiento de efectos seleccionados](paper/lazy_values_note.es.md)
corrige las referencias de historial y las lecturas antiguas pendientes durante efectos posteriores.
La [campaña posterior](audit-evidence/c-priority-integration-v1/README.es.md) completa 2332
comprobaciones de historial tanto en normal como con ASan/UBSan. La campaña histórica de 355 casos
dirigidos conserva su alcance original; la validación integrada completa continúa pendiente.

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
[evidencia finita](audit-evidence/fixed-memory-loops-v1/README.es.md) no completa los direcciones
generales, retornos ni flujos arbitrarios de propiedad. Los
[tamaños CPU de ejecución](c11-runtime-memory.md) ya seleccionan prefijos de buffers fijos
comprobados; `buffer_lengths.c` pasa esa longitud a una función y observa los resultados.

La [corrección de préstamos de lectura](c11-readonly-borrows.md) admite alias `const` en parámetros
escalares y de buffers fijos. Rechaza las escrituras por casts antes de perder el ámbito del
préstamo. Su evidencia de 239 entradas distingue los casos comprobados de las combinaciones
generales de alias y vidas que siguen pendientes.

La [ampliación de retornos CPU](c11-procedure-returns.md) implementa salidas anticipadas de
procedimientos `void` en bloques y bucles, con limpieza léxica y continuación del llamador. Sus
estados, fases e inversas tienen evidencia propia; los resultados propios anticipados y el CFG
general siguen pendientes.

La [composición con un llamador coherente](paper/coherent_returns_note.es.md) ya admite esos
retornos cuando los predicados CPU son puros. No habilita salidas decididas por datos cuánticos,
efectos CPU observables, bucles coherentes ni vidas generales.

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
promete recursos ilimitados, toda la biblioteca C11 ni ejecución demostrada en una QPU física. El
estado actual todavía no cumple ese cierre.

Los contratos precisos se conservan en la [matriz de capacidades](c-coverage.md), los
[bloques entregados](entregas-incrementales.es.md) y el
[estado general](estado-general-del-proyecto.es.md).

Los [locales CPU privados y bucles](paper/private_cpu_loops_note.es.md) admiten inicializadores
puros, escrituras dentro de la misma rama cuántica de declaración y condiciones de iteración CPU
puras. Se excluyen direcciones tomadas, almacenamiento estático, `volatile`, `_Atomic`, punteros y
agregados privados. Los [saltos CPU estructurados](paper/loop_jumps_note.es.md) añaden `break` y
`continue`, con la limpieza y los incrementos de C. Quedan decisiones cuánticas de salto o
iteración, `continue` a través de `switch`, efectos CPU anidados y vidas/alias/ABI generales. Las
notas y cápsulas anteriores mantienen sus propias identidades y alcances.
