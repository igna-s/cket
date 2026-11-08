# Alcance actual: C cotidiano funcional

**Acordado el 23 de octubre de 2026.** El objetivo actual es poder escribir programas completos con
las herramientas habituales de C: funciones, tipos comunes, control, arrays, punteros y vectores. La
compatibilidad debe funcionar en conjunto dentro de un perfil expl??cito. La idea de ??el 50% que
us??s siempre?? expresa esta prioridad; no es una cifra de cobertura medida.

El inventario de todas las interfaces C11 se conserva como referencia t??cnica y posible
ampliaci??n. Completar cada una de sus entradas deja de ser condici??n para cerrar este alcance. Una
declaraci??n aceptada, un diagn??stico correcto o un ejemplo que solo compila tampoco completan una
capacidad.

## Qu?? debe poder usarse

| Parte del n??cleo         | Resultado que se busca                                                                                             | Estado actual y trabajo pendiente                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ------------------------- | ------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Tipos y operadores        | `bool`, caracteres, enteros con y sin signo, aliases, `size_t`, conversiones y operadores habituales.              | Hay representaci??n y s??ntesis entera hasta 64 bits. Faltan combinaciones de contextos, divisi??n general y cierre de recursos; no est??n completos todos los usos de cada tipo.                                                                                                                                                                                                                                                                         |
| Funciones                 | Par??metros, llamadas entre funciones, resultados, locales y funciones que procesan buffers.                       | Composici??n directa en la misma unidad, pr??stamos parciales y funciones de valor con cl??usulas de guarda / m??ltiples `return` estructurados. Faltan usos generales de punteros, retornos con efectos laterales complejos y vidas; la ABI entre unidades conserva restricciones.                                                                                                                                                                       |
| Control                   | Condiciones, `switch`, `for`, `while`, `do while` y salidas habituales con efectos y limpieza correctos.           | Hay condiciones coherentes, `switch` estructurados (cl??sicos CPU y coherentes cu??nticos con `case`, rangos, `default`, `break`, `continue` y `return`), bucles acotados y control CPU. Faltan combinaciones generales de CFG e historia; un bucle con condici??n cu??ntica general sigue pendiente.                                                                                                                                                     |
| Arrays y structs b??sicos | Vectores, matrices, campos enteros, recorridos, lecturas y actualizaciones.                                        | Arrays y productos fijos implementados con l??mites. La nueva lectura/escritura con ??ndice cu??ntico admite arrays escalares de 1 a 64 elementos con ejecuciones y evidencia finita de estados e inversas.                                                                                                                                                                                                                                               |
| Punteros y buffers        | Pasar un buffer y su longitud, desreferenciar, indexar, crear alias y escribir conservando procedencia y l??mites. | Hay pr??stamos comprobados con funciones encadenadas e ??ndices cu??nticos sobre escalado can??nico; hay lecturas con ??ndices CPU y recorridos con acumulaci??n y retorno. Se a??aden heaps nativos con tama??o CPU, alias y pr??stamos de funciones. Faltan m??s formas de direcciones/vidas e integraci??n general.                                                                                                                                    |
| Vectores din??micos       | Reservar almacenamiento, mantener longitud/capacidad, recorrer, actualizar y liberar con una vida definida.        | Implementaci??n parcial de `malloc`, `calloc`, `realloc` y `free`, capacidad CPU, pr??stamos y recorridos. `dynamic_heap.c` usa almacenamiento que crece realmente. Se a??aden escrituras con ??ndice cu??ntico y resultados de asignaci??n/incremento. Los resultados dentro de bucles CPU y las copias heap???fijo a??aden historia por iteraci??n. Faltan pruebas generales de direcciones disjuntas y memoria/ABI generales. [Contrato](c11-heap.md). |
| Biblioteca frecuente      | Copiar, mover y llenar memoria; manejar cadenas y conversiones habituales dentro de contratos comprobados.         | Existen adaptadores parciales. Las nuevas conversiones decimales, escrituras por b??squeda e indexaci??n coherente tienen ejecuciones y pruebas finitas; sus dominios generales siguen pendientes.                                                                                                                                                                                                                                                        |
| C??lculo real habitual    | `float` y `double` cu??nticos, operaciones y conversiones con sem??ntica definida.                                 | Pendiente. Los flotantes cl??sicos de GCC y los ??ngulos de las puertas no implementan esos tipos cu??nticos. Se abordan despu??s de integrar el n??cleo entero, funciones y memoria.                                                                                                                                                                                                                                                                     |
| Entrada/salida            | Leer datos cl??sicos, preparar entradas, medir expl??citamente y mostrar o guardar resultados.                     | Frontera CPU existente; debe componerse con los programas del perfil sin mediciones ocultas.                                                                                                                                                                                                                                                                                                                                                              |

La ciberseguridad, criptograf??a y autenticaci??n permanecen cl??sicas. El cierre de este perfil no
requiere completar caracteres anchos, complejos, atomics, threads ni todas las interfaces poco
habituales. Sus entradas pendientes siguen visibles en el inventario.

El bloque posterior a RC3 corrige la coherencia de bytes intactos en cuatro adaptadores fijos de
cadenas. La [nota](paper/string_writes_note.es.md) delimita los programas comprobados y los casos
que exceden los recursos. El [seguimiento de efectos seleccionados](paper/lazy_values_note.es.md)
corrige las referencias de historial y las lecturas antiguas pendientes durante efectos posteriores.
La [campa??a posterior](audit-evidence/c-priority-integration-v1/README.es.md) completa 2332
comprobaciones de historial tanto en normal como con ASan/UBSan. La campa??a hist??rica de 355 casos
dirigidos conserva su alcance original; la validaci??n integrada completa contin??a pendiente.

El avance del 24 de octubre a??ade `memcpy` y `memmove` entre heaps y `memset` sobre heaps, con
longitudes CPU de ejecuci??n, vistas de bytes parciales y retornos de direcci??n cl??sicos. Se
conservan los originales para los movimientos solapados y las inversas. Tambi??n se copian buffers
cu??nticos fijos y heaps en ambas direcciones, y objetos CPU conocidos hacia heaps. El destino fijo
conserva sus qubits fuera de la longitud solicitada. Sus escrituras dentro de bucles CPU y los
resultados de incremento del heap a??aden historia por iteraci??n. Las cadenas sobre heap siguen
pendientes; el [contrato](c11-heap.md) detalla los contextos y recursos.

Las copias, movimientos solapados y rellenos entre buffers fijos tambi??n conservan versiones dentro
de bucles CPU. [fixed_buffers.c](../examples/blocks/05/fixed_buffers.c) combina esas operaciones en
una funci??n con contador por valor. La
[evidencia finita](audit-evidence/fixed-memory-loops-v1/README.es.md) no completa los direcciones
generales, retornos ni flujos arbitrarios de propiedad. Los
[tama??os CPU de ejecuci??n](c11-runtime-memory.md) ya seleccionan prefijos de buffers fijos
comprobados; `buffer_lengths.c` pasa esa longitud a una funci??n y observa los resultados.

La [correcci??n de pr??stamos de lectura](c11-readonly-borrows.md) admite alias `const` en
par??metros escalares y de buffers fijos. Rechaza las escrituras por casts antes de perder el
??mbito del pr??stamo. Su evidencia de 239 entradas distingue los casos comprobados de las
combinaciones generales de alias y vidas que siguen pendientes.

La [ampliaci??n de retornos CPU](c11-procedure-returns.md) implementa salidas anticipadas de
procedimientos `void` en bloques y bucles, con limpieza l??xica y continuaci??n del llamador. Sus
estados, fases e inversas tienen evidencia propia; los resultados propios anticipados y el CFG
general siguen pendientes.

La [composici??n con un llamador coherente](paper/coherent_returns_note.es.md) ya admite esos
retornos cuando los predicados CPU son puros. No habilita salidas decididas por datos cu??nticos,
efectos CPU observables, bucles coherentes ni vidas generales.

La ampliaci??n del 28 de octubre habilita `switch` estructurados (cl??sicos CPU y coherentes
cu??nticos con casos simples, rangos, casos apilados, `default`, `break`, `continue` a trav??s del
`switch` y `return`), funciones `quant` de valor con cl??usulas de guarda y m??ltiples `return`, y
la composici??n de `memcpy`, `memmove`, `memset` y parches parciales con bucles CPU, `break`,
`continue` y `return;` bajo un llamador coherente `if (control)`, verificando matrices densidad
reducidas del byte intacto e inversas completas. En noviembre de 2026 se a??aden adem??s punteros y
par??metros `quant` a estructuras (`p->campo`, `p[i].campo`), movimiento, asignaci??n y `reset` de
productos completos (`quant struct S b = a;`, `d = b;`, `reset(m)`), operaciones de memoria/cadenas
(`memcpy`, `memmove`, `memset`, `memcmp`, `strncmp`, `strncpy`, `strncat`, `memchr`, `strlen`, etc.)
sobre estructuras sin relleno, matrices multidimensionales y longitudes de bytes `quant`,
conversiones de cadena a entero en base arbitraria `2..36` (`strtol`, `strtoll`, `strtoimax` con
`endptr` nulo o `&end` local adem??s de `atoi`, `atol`, `atoll`), intervalos de preparaci??n
di??dicos desplazados `superposition(a, a + 2^k - 1)` con certificado de intervalo de soporte para
divisiones y rangos, indexaci??n coherente multi-eje con ??ndices cu??nticos sobre matrices,
arreglos de estructuras y punteros de b??squeda (`m[q_i][q_j]`, `t[q_i].arr[q_j]`, `p[q_i]`,
`*(p + q_i)`), inicializadores de arreglos de bytes anidados en estructuras/matrices, bucles `for`,
`while` y `do-while` est??ticamente acotados con guarda cu??ntica pura, contadores predeclarados y
pasos constantes arbitrarios (`+= k`, `-= k`, `!=`), propagaci??n nativa de `quant` a trav??s de
`typedef`, y funciones `quant` con resultado propio imperativo (escalar o `struct`), bucles CPU y
salidas anticipadas. V??ase tambi??n el [??ndice maestro del c??digo](indice-del-codigo.es.md).

## Orden de trabajo

1. Integrar los enteros y sus consumidores con funciones, arrays y structs b??sicos.
1. Completar el recorrido habitual de punteros y buffers, incluidos alias y resultados de funciones.
1. Incorporar almacenamiento y vida de vectores din??micos, con l??mites de recursos declarados.
1. Completar las operaciones frecuentes de memoria/cadenas que necesiten esos programas y despu??s
   el c??lculo habitual con `float` y `double`.
1. Cerrar la validaci??n integrada y ajustar documentaci??n y afirmaciones de los papers al
   resultado.

Se entrega capacidad utilizable por bloques. El usuario abri?? ahora la validaci??n de propiedad,
copias, fases e inversas. La [campa??a dirigida](audit-evidence/ownership-contracts-v1/README.es.md)
ejecuta siete ejemplos recientes y comprueba estados de funciones, buffers, ??ndices, historia y
conversiones. La evidencia finita nueva no completa la integraci??n general de las filas anteriores.

## C??mo se determina que funciona

El cierre debe incluir programas completos que combinen las capacidades, con una referencia C
ordinaria para los resultados definidos. Los casos de aceptaci??n deben incluir:

- Funciones que reciben un vector y su longitud, recorren sus elementos, acumulan y devuelven un
  resultado; llamadas encadenadas con locales y resultados intermedios.
- Arrays y matrices con lecturas y escrituras, ??ndices CPU y cu??nticos dentro del contrato,
  structs b??sicos y valores producidos por asignaciones e incrementos.
- Buffers compartidos entre funciones, alias admitidos, copias y movimientos de memoria, cadenas y
  conversi??n de datos, conservando los valores que siguen vivos.
- Un vector con almacenamiento din??mico que se reserva, se usa y se libera seg??n su contrato; un
  nombre de ejemplo o un array fijo no sustituye esta capacidad.
- Combinaciones de condiciones y bucles que ejerciten limpieza de temporales, historia y retornos;
  medici??n expl??cita en la frontera de salida.

Para datos cu??nticos, comprobar solo n??meros medidos no alcanza: tambi??n hacen falta casos de
fase, correlaci??n e inversa, contrastes independientes apropiados y agotamiento de recursos
declarado. La validaci??n se concentra en este perfil y sus combinaciones. Cada resultado debe
identificar las fuentes que realmente se ejecutaron.

??100% funcional?? significa cumplir los contratos de este perfil y sus programas integrados. No
promete recursos ilimitados, toda la biblioteca C11 ni ejecuci??n demostrada en una QPU f??sica. El
estado actual todav??a no cumple ese cierre.

Los contratos precisos se conservan en la [matriz de capacidades](c-coverage.md), los
[bloques entregados](entregas-incrementales.es.md) y el
[estado general](estado-general-del-proyecto.es.md).

Los [locales CPU privados y bucles](paper/private_cpu_loops_note.es.md) admiten inicializadores
puros, escrituras dentro de la misma rama cu??ntica de declaraci??n y condiciones de iteraci??n CPU
puras. Se excluyen direcciones tomadas, almacenamiento est??tico, `volatile`, `_Atomic`, punteros y
agregados privados. Los [saltos CPU estructurados](paper/loop_jumps_note.es.md) a??aden `break` y
`continue`, con la limpieza y los incrementos de C. Quedan decisiones cu??nticas de salto o
iteraci??n, `continue` a trav??s de `switch`, efectos CPU anidados y vidas/alias/ABI generales. Las
notas y c??psulas anteriores mantienen sus propias identidades y alcances.
