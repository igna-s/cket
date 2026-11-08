# Qu?? est?? hecho y qu?? falta en Quantum / `|c>`

**Estado actualizado el 5 de noviembre de 2026.** Este documento resume el ??rbol principal y
distingue los cambios que todav??a est??n en pruebas. Es una fotograf??a del estado actual; los
informes de cada campa??a conservan sus resultados y fuentes originales. Para consultar r??pidamente
d??nde est?? cada subsistema, funci??n e instrucci??n del IR cu??ntico sin recorrer todo el
repositorio, v??ase el [??ndice maestro del c??digo](indice-del-codigo.es.md).

**Punteros a `struct`, indexaci??n coherente multi-eje, bucles `for`/`while`/`do` con guarda
cu??ntica, intervalos `superposition(a, a+2^k-1)`, `strtol` con `endptr`, tama??os `quant` en
`<string.h>` y resultados propios imperativos:** se incorporan pr??stamos comprobados de estructuras
(`struct` con calificador `quant`) en par??metros y locales (`p->campo`, `p[i].campo`), movimiento,
asignaci??n y `reset` de productos completos (`quant struct S b = a;`, `d = b;`, `reset(m)`),
operaciones de memoria/cadena (`memcpy`, `memmove`, `memset`, `memcmp`, `strncmp`, `strncpy`,
`strncat`, `memchr`, `strlen`, `atoi`, `atol`, `atoll`, `strtol`, `strtoll`, `strtoimax` en bases
`2..36` con `endptr` nulo o `&end` local) sobre estructuras sin relleno, matrices multidimensionales
y conteos de bytes `quant`, intervalos de superposici??n di??dicos desplazados
`superposition(a, a + 2^k - 1)` con certificado de soporte en el motor, lectura y escritura
coherente con m??ltiples ??ndices cu??nticos (`m[q_i][q_j]`, `t[q_i].arr[q_j]`) y sobre punteros de
b??squeda (`p[q_i]`, `*(p + q_i)`), expansi??n de cadenas/arreglos de bytes anidados en
inicializadores de estructuras y matrices, desenrollado est??tico de bucles `for`, `while` y
`do-while` acotados con guarda cu??ntica pura, contadores predeclarados y pasos constantes (`+= k`,
`-= k`, `!=`), propagaci??n nativa de `quant` a trav??s de `typedef`, y funciones `quant` de
resultado propio imperativo (escalar o `struct`) con bucles CPU, salidas anticipadas y asignaci??n a
registros existentes (`408` comprobaciones nativas en `build/cket_tests`).

**Flujo C cotidiano (`switch` y retornos con guardas) y memoria con llamador coherente:** se
implementa la expansi??n estructurada y reversible de sentencias `switch` (tanto cl??sicas CPU
`switch (n)` como coherentes cu??nticas `switch (q)`, con `case LOW:`, rangos `case LOW ... HIGH:`,
etiquetas apiladas, `default:`, `break;`, `continue;` hacia el bucle exterior y `return;` /
`return expr;`, rechazando expl??citamente el *fallthrough* no estructurado con efectos cu??nticos)
y las funciones `quant` de valor con cl??usulas de guarda y m??ltiples `return`. Adem??s, se corrige
la limpieza de definiciones previas en [`register_patch`](../src/engine/heap.inc) para parches
parciales de registros revinculados (`55155` comprobaciones en `validate_patches`) y se califica la
composici??n de `memcpy`, `memmove` (adelante/atr??s), `memset` y parches parciales con bucles CPU,
`break`, `continue` y `return;` bajo un llamador coherente `if (control)`: `9480` puntos de estado,
`2400` inversas completas y `1896` comprobaciones de matriz densidad reducida del byte intacto en
modo normal (`O0`/`O2`), y `4740`, `1200` y `948` con ASan/UBSan (`O1`).

**Candidata actual: 1.0.0-rc.3. La 1.0 estable sigue pendiente.** La
[gu??a de versi??n](releases/v1.0.0-rc.3.es.md) resume lo implementado y lo que falta. La
[demo de buffer fijo](../output/demo/README-fixed.es.md) est?? lista para Windows/WSL. La
ciberseguridad permanece cl??sica y sin cambios.

**Compilador actualizado listo para usar:** el [SDK nuevo](../output/sdk/loop-jumps/README.es.md)
incluye el plugin corregido y 37 programas reconstruidos. Ejecut??
`output\sdk\loop-jumps\run_coherent_demo.cmd` para compilar el ejemplo de retorno, limpieza e
interferencia. La [gu??a de entrega](entrega-utilizable-2026-10-26.es.md) resume comandos y
pendientes.

**Memoria con saltos CPU verificada:** `memcpy`, `memmove` y `memset` conservan m??scaras, historia
y fases en cinco formas de salto/bucle. La [nota](paper/memory_loop_jumps_note.es.md) y su
[c??psula](audit-evidence/memory-loop-jumps-v1/README.es.md) registran 4740 puntos de estado y 1200
inversas en normal; 2370 y 600 instrumentados. La densidad del bit intacto de una palabra parcial
tambi??n se conserva; se rechaza un mutante que lo copia a un auxiliar. La campa??a tiene 245
entradas y reutiliza exactamente los binarios de `5f12080`: el SDK recomendado conserva su
identidad. No se ampl??a su soporte de operaciones ni se afirma que estas familias de memoria ya
est??n calificadas con un llamador coherente exterior.

**CI completo de llamadas coherentes aprobado:** la
[c??psula de `cba9759`](audit-evidence/full-ci-coherent-returns-v1/README.es.md) conserva 59
artefactos, logs y la comprobaci??n de 244 blobs Git. Cierra el pendiente de aquella revisi??n; los
locales, bucles y saltos posteriores mantienen sus propios CI y fuentes.

**Break y continue CPU implementados:** se transportan con permisos privados y se llega a la
limpieza del bloque antes de salir o continuar. `continue` conserva el incremento del `for`; `break`
lo omite. La [nota](paper/loop_jumps_note.es.md) y la
[evidencia](audit-evidence/loop-jumps-v1/README.es.md) registran 1900 puntos de estado y 950
inversas con llamador coherente en normal; 380 y 190 con ASan/UBSan. La comparaci??n con C nativo
pasa 1100 ejecuciones normales y 220 instrumentadas. El SDK recomendado incluye el ejemplo. Quedan
saltos/iteraci??n cu??nticos, `continue` a trav??s de `switch`, flujo y vidas/alias/ABI generales e
integraci??n completa de estas fuentes.

**Bloque anterior de locales mutables y bucles CPU:** las llamadas coherentes admiten `for`, `while`
y `do while` con condiciones CPU puras, contadores privados y retornos CPU. Las escrituras quedan
dentro de la misma rama cu??ntica donde se declar?? cada local. La
[nota](paper/private_cpu_loops_note.es.md) y la
[evidencia](audit-evidence/private-cpu-loops-v1/README.es.md) registran 1400 puntos de estado y 700
inversas en normal; 280 y 140 con ASan/UBSan. El SDK recomendado comprueba tambi??n un par??metro
CPU modificado por valor. Quedan condiciones/salidas cu??nticas, saltos coherentes, efectos CPU
anidados, vidas/alias/ABI generales e integraci??n completa de estas fuentes.

**Bloque anterior de locales CPU inmutables:** se admiten `bool` y enteros autom??ticos
inicializados con expresiones CPU puras, sin escrituras posteriores ni direcci??n tomada, dentro de
llamadas coherentes. La [nota](paper/private_cpu_locals_note.es.md) y la
[evidencia](audit-evidence/immutable-cpu-locals-v1/README.es.md) registran 800 puntos de estado y
400 inversas en normal; 160 y 80 con ASan/UBSan. El SDK recomendado incluye este plugin y el nuevo
ejemplo de aquel bloque. La ampliaci??n de bucles de arriba supera los l??mites de mutabilidad y
bucles CPU; sus propias fuentes y resultados se identifican por separado.

**Integraci??n completa aprobada para `ac8f5bc`:** formato y toda la composici??n de
`scripts/verify.sh` pasan en normal y ASan/UBSan sobre las 242 entradas identificadas del SDK
anterior. La [c??psula de CI](audit-evidence/full-ci-cpu-returns-v1/README.es.md) conserva los logs
y vincula los hashes con los blobs Git. Esto cierra el pendiente de integraci??n de aquellas
fuentes. El compilador de llamadas coherentes `cba9759`, anterior al bloque de locales privados,
tiene ahora su [CI completo aprobado](audit-evidence/full-ci-coherent-returns-v1/README.es.md). Las
menciones de CI pendiente m??s abajo pertenecen a campa??as anteriores.

**Retornos desde un llamador coherente:** los predicados CPU puros de salida ya seleccionan puertas
bajo el control cu??ntico del llamador. Siguen rechazados los efectos CPU observables y las salidas
decididas por datos cu??nticos sin medir. La [nota t??cnica](paper/coherent_returns_note.es.md) y su
[evidencia de 244 entradas](audit-evidence/coherent-returns-v1/README.es.md) registran estados,
fases e inversas normales e instrumentados. El SDK nuevo incluye el ejemplo de interferencia.

**Retornos CPU implementados:** los procedimientos `void` admiten `return;` anticipado en bloques,
condiciones y bucles CPU. Terminan su limpieza l??xica y contin??an el llamador. Tambi??n se corrige
un rechazo de ramas CPU causado por certificados de valores conocidos fuera de su camino. La
[nota](c11-procedure-returns.md) y su
[evidencia de 242 entradas](audit-evidence/procedure-returns-v1/README.es.md) distinguen resultados
C, efectos, fases, auxiliares limpios e inversas de los flujos generales todav??a pendientes.

**Entradas de funciones comprobadas:** se rechazan prototipos cl??sicos y punteros que pod??an
alcanzar un stub vac??o de una funci??n `quant`. La entrada nativa restante devuelve un error
expl??cito en lugar de simular ??xito. La [nota](c11-function-entries.md) y su
[evidencia de 240 entradas](audit-evidence/function-entries-v1/README.es.md) registran 90 contratos
normales e instrumentados, regresiones y el SDK reconstruido.

**Referencia de memoria runtime corregida:** se reproduce el fallo del CI anterior,
`unmodelled retained version`: faltaba modelar una guarda privada cero en un `memset` de longitud
cero. La [nueva evidencia](audit-evidence/runtime-memory-reference-v1/README.es.md) registra 900
puntos de estado complejo y 270 inversas completas, normales e instrumentadas. Rechaza adem??s tres
mutaciones unitarias de datos, fase y guarda. Aquella correcci??n no cambi?? el compilador ni sus
binarios; el SDK anterior conserva su referencia original. El SDK nuevo incluye la referencia
corregida. Se cierra ese fallo concreto; la integraci??n completa sigue pendiente.

**Continuaci??n de integraci??n:** las dos matrices siguientes, escrituras coherentes y buffers
fijos en bucles, pasan en normal y ASan/UBSan: 540 puntos de estado y 210 inversas completas en la
[nueva evidencia](audit-evidence/memory-integration-tail-v1/README.es.md). La
[integraci??n de `183169a`](https://github.com/igna-s/cket/actions/runs/40500449856) se inici?? el
27 de octubre; al guardar este seguimiento, formato y contratos del formateador pasaron y la
verificaci??n general sigue en curso.

**Pr??stamos de lectura compartidos:** se admite pasar el mismo due??o a varios par??metros `const`;
se siguen rechazando solapamientos mutables. Se corrige adem??s una escritura por cast que escapaba
del ??mbito de lectura. La [nota](c11-readonly-borrows.md) y la
[evidencia de 239 entradas](audit-evidence/readonly-borrows-v1/README.es.md) registran 305
comprobaciones dirigidas, bindings (519) y n??cleo (395) normales e instrumentados, y 1509 de
ejemplos normales. Los recuentos de las campa??as anteriores que siguen abajo conservan sus propias
fuentes; no recalifican autom??ticamente este plugin. La integraci??n completa sigue abierta.

**Bloque de desarrollo posterior a RC3:** las escrituras fijas de `strcpy`, `strncpy`, `strcat` y
`strncat` usan m??scaras de los bytes realmente escritos. Un NUL cu??ntico selecciona esa m??scara
sin medirlo; las concatenaciones vac??as no guardan una historia de escritura. La
[nota de cadenas](paper/string_writes_note.es.md) explica la no clonaci??n, los productores y las
exclusiones de recursos. Las matrices normales O0/O2 y ASan/UBSan O1 registran 1776 densidades
p??blicas, 1776 estados conjuntos por puertas independientes y 888 inversas, con ??mbitos distintos.
Las 24 comprobaciones adicionales de NUL inv??lido y guarda inactiva verifican el estado anterior a
cualquier escritura. La 1.0 estable sigue pendiente.

**Historial y efectos seleccionados corregidos:** la referencia antigua supon??a identificadores
separados para el valor viejo y el destino actual. Se actualizan los canales, estados completos y
fases sin depender de esa suposici??n. La suite completa pasa 2332 comprobaciones tanto en normal
como con ASan/UBSan en la [nueva campa??a C](audit-evidence/c-priority-integration-v1/README.es.md).
La pasada hist??rica de 355 casos instrumentados de la [nota t??cnica](paper/lazy_values_note.es.md)
conserva su alcance original. Al llegar a los casos antes ocultos se corrigieron operandos
anteriores que no se transportaban y vidas de inversas en brazos inactivos. El CI general sigue
pendiente.

**Campa??a anterior de 237 entradas:** pasan los 17 validadores C normales listados en la nueva
c??psula y diez completos con ASan/UBSan. La pasada instrumentada de capturas de bytes se interrumpe
por tiempo; los seis validadores que ven??an despu??s quedan pendientes. En normal tambi??n pasan
145 compilaciones del fuzzer, 24 circuitos de crecimiento, cinco comparaciones cl??sicas y 70
exportaciones verificadas con QuEST: 64 estados y seis canales. Se corrige una expectativa antigua
del fixture QASM para ramas CPU sin par??metros num??ricos. El SDK conserva exactamente los mismos
binarios del compilador. Esta campa??a no sustituye toda la composici??n de `scripts/verify.sh` ni
cierra la 1.0 estable.

**Nueva demo lista para ejecutar:** `output\demo\run_lazy_demo.cmd` usa WSL sin recompilar ni montar
unidades Windows. Incluye el caso antes rechazado `condition ? value++ : value--` y otro con
entradas en superposici??n y fases. La [gu??a](../output/demo/README-lazy.es.md) distingue estos
valores observados de la evidencia independiente de coherencia e inversas.

**Se corrigen las copias coherentes del heap del contraejemplo anterior:** pureza sectorial 1 ??? 1
y error de densidad cero. Las escrituras parciales del heap intercambian solamente los bits
escritos; el byte intacto conserva coherencia en ambas ramas. Las copias solapadas leen las
expresiones anteriores guardadas y los ??ndices cu??nticos usan controles por elemento. La
[c??psula](audit-evidence/heap-coherence-v1/README.es.md) conserva 233 entradas y registra 2670
comparaciones de densidad p??blica, 2670 contrastes de estado conjunto por puertas independientes y
852 inversas, con construcciones normales y ASan/UBSan. Son ??mbitos distintos, no cobertura
porcentual de C.

**Se corrige el contraejemplo del byte intacto de buffers fijos:** pureza conjunta 1 ??? 1 en ambas
ramas y error de densidad cero. La [nueva c??psula](audit-evidence/fixed-patches-v1/README.es.md)
conserva 236 entradas exactas y registra 1920 comparaciones de densidad p??blica, 1920 estados
conjuntos por puertas independientes y 912 inversas, normales O0/O2 y ASan/UBSan O1. Incluye ra??ces
y productores reversibles, enteros 16/32 con/sin signo, longitudes CPU, alias, solapamiento y
pr??stamos con fases despu??s de cambiar la longitud. Los temporales y sus predicados se limpian
cuando terminan sus consumidores. La [nota t??cnica](paper/fixed_patches_note.es.md) explica los
contratos y sus l??mites.

**Qu?? sigue pendiente:** cerrar vidas y alias generales, composici??n y el perfil cotidiano;
ampliar los adaptadores de cadenas y resolver los perfiles excluidos por recursos; calificar
recursos de 64 bits y m??s combinaciones de escrituras repetidas. Flotantes, papers y CI general
conservan sus pendientes. Los resultados finitos anteriores no son un porcentaje de C ni certifican
toda la biblioteca. El [informe](re-audit-2026-10-24-coherence.md) conserva cada revisi??n
hist??rica.

**Mejora de composici??n de memoria:** un `memset` cuyo valor compara el mismo destino baja de 26 a
18 cables f??sicos y ya ejecuta en el simulador denso. La [nota](paper/single_fill_note.es.md)
explica la captura evitada, las variantes descartadas por coherencia o recursos y la calificaci??n
normal e instrumentada. La integraci??n general conserva sus gates pendientes.

La parada posterior de `validate_memory_returns`, `unexpected memory return word history`, se cierra
con la [referencia por m??scara](audit-evidence/memory-return-reference-v1/README.es.md): pasan 290
comprobaciones de retornos y 170 de distancias en normal y con sanitizadores. La referencia verifica
los canales C sin exigir copias de bytes intactos e identifica por separado las inversas de puertas
y las guardadas. La integraci??n general sigue pendiente.

**Entrega lista para ejecutar:** desde la ra??z en Windows, `output\demo\run_memory_demo.cmd`
ejecuta tres casos verificados sin recompilar, mediante WSL. La [gu??a](../output/demo/README.es.md)
y la [nota t??cnica de dos p??ginas](../output/pdf/cket_memoria_runtime_nota_tecnica.pdf) incluyen
el programa, las salidas reales y los l??mites. Es una entrega funcional del bloque de memoria; el
n??cleo cotidiano contin??a en desarrollo.

**Longitudes CPU de memoria fija:** `memcpy`, `memmove` y `memset` seleccionan prefijos con tama??o
recibido en ejecuci??n, dentro de objetos comprobados y hasta 64 bytes. El
[ejemplo](../examples/blocks/05/buffer_lengths.c) pasa la longitud de `argv` a una funci??n. Se
comprueban l??mites y solapamiento antes de escribir. Se corrigen adem??s una uni??n de ramas que
invalidaba un due??o del otro brazo y una lectura directa del retorno que pod??a obtener un
placeholder CPU. La lectura con `measure` usa el registro real; los accesos CPU sin observaci??n se
rechazan. El [contrato](c11-runtime-memory.md) conserva los recursos y consumidores pendientes.

**Memoria fija dentro de bucles:** `memcpy`, `memmove` solapado y `memset` usan ahora historial por
iteraci??n tambi??n entre buffers fijos. Un relleno que lee el destino se captura antes de modificar
la primera palabra. El [nuevo ejemplo](../examples/blocks/05/fixed_buffers.c) pasa un array a una
funci??n y combina movimiento y relleno dentro de `while(rounds--)`, con 0 a 3 iteraciones. La
[campa??a de memoria fija](audit-evidence/fixed-memory-loops-v1/README.es.md) registra 300 puntos de
estado completo, 90 inversas, 32 ejecuciones adicionales y las regresiones de copia/relleno, en
construcciones normales y ASan/UBSan. Ese cierre usaba tama??os y direcciones constantes; la
ampliaci??n de longitudes CPU anterior conserva sus direcciones comprobadas.

**Nuevo cierre de bucles y funciones:** los resultados de `p[i]++`, las asignaciones escalares y las
copias heap???buffer fijo conservan ahora sus versiones en cada iteraci??n CPU. Tambi??n se corrige
el fallo interno al copiar a un par??metro de buffer dentro de una funci??n con `while(n--)`; sus
argumentos CPU modificables tienen copias locales. El
[ejemplo de iteraciones](../examples/blocks/05/heap_iterations.c) combina incremento, acumulaci??n y
copia. La [campa??a de este bloque](audit-evidence/runtime-history-v1/README.es.md) registra 300
puntos de estado complejo completo, 90 inversas completas y 32 ejecuciones adicionales de contextos
nativos, con construcciones normales y ASan/UBSan. Son casos finitos; siguen pendientes la propiedad
en flujos generales, m??s consumidores de punteros, cadenas sobre heap y flotantes.

**Avance del bloque 05:** se implementan heaps cu??nticos nativos con `malloc`, `calloc`, `realloc`
y `free`. El nuevo [programa din??mico](../examples/blocks/05/dynamic_heap.c) recibe la capacidad
por `argv`, reserva, crece, recorre mediante una funci??n y libera. La carga sigue en registros
cu??nticos; los punteros y capacidades son cl??sicos. Crecer conserva los registros; liberar no mide
ni borra estados desconocidos. El [contrato](c11-heap.md) detalla recursos y contextos pendientes.
La [campa??a del heap](audit-evidence/heap-effects-v1/README.es.md) conserva los estados completos y
las inversas comprobadas. Se a??aden escrituras con ??ndice cu??ntico y resultados de asignaci??n e
incremento que conservan sus versiones anteriores, incluso despu??s de `free`. Es un avance parcial
del n??cleo cotidiano.

**Nuevo bloque de memoria del heap:** `memcpy` y `memmove` entre heaps, y `memset` sobre heaps,
aceptan tama??os CPU de ejecuci??n y vistas de bytes parciales. El nuevo
[ejemplo](../examples/blocks/05/heap_bytes.c) copia valores en superposici??n, mueve regiones
solapadas y rellena con un valor cu??ntico capturado. Se conservan las versiones originales y las
fases; los retornos son direcciones cl??sicas con alias comprobados. El [contrato](c11-heap.md)
mantiene expl??citos los l??mites. Las cadenas sobre heap y los flotantes cu??nticos siguen
pendientes. La [evidencia de este bloque](audit-evidence/heap-memory-v1/README.es.md) conserva 256
puntos de estado completo, 64 inversiones completas de puertas y las comprobaciones con ASan/UBSan.

**Integraci??n de buffers fijos y heap:** `memcpy` y `memmove` copian ahora en ambas direcciones,
con longitudes CPU y vistas fijas comprobadas. Tambi??n se capturan bytes actuales de objetos CPU
conocidos hacia el heap. El [ejemplo](../examples/blocks/05/heap_transfer.c) conserva los resultados
despu??s de cambiar el origen y liberar el heap. Las palabras fuera de la longitud conservan sus
qubits, incluida una copia de longitud cero. Las escrituras heap???fijo y los resultados de
incremento del heap a??aden ahora [historia por iteraci??n](c11-runtime-history.md). El nuevo
[ejemplo](../examples/blocks/05/heap_iterations.c) combina acumulaci??n de postfix y copia dentro de
un bucle con longitud CPU, incluida longitud cero. Los usos generales de punteros y ABI siguen
abiertos. La [evidencia de integraci??n](audit-evidence/heap-transfer-v1/README.es.md) registra 224
puntos de estado completo y 56 inversas, incluidas las construcciones con ASan/UBSan.

**Avance de propiedad y copia:** el usuario abri?? esta etapa de validaci??n. Se a??aden lecturas de
buffers con ??ndice CPU, acumulaci??n/resta y paridad en bucles CPU, y resultados escalares
devueltos por funciones. La
[campa??a de propiedad](audit-evidence/ownership-contracts-v1/README.es.md) separa ejecuciones,
estados completos, fases, inversas y sanitizadores. Corrige adem??s una suma que pod??a ocultar un
desbordamiento con signo al estrechar el resultado. Los
[contratos de no clonaci??n](no-clonacion.es.md) explican qu?? significan copiar, prestar y mover.

**Alcance vigente:** completar el [C cotidiano funcional](perfil-c-cotidiano.es.md): tipos comunes,
funciones, control, arrays, punteros y vectores, con las operaciones de biblioteca que necesitan. El
usuario aclar?? este alcance el 23 de octubre. El inventario completo de C11 queda como referencia
ampliada; completar todas sus entradas deja de ser requisito para este objetivo. El n??cleo
cotidiano todav??a tiene pendientes de implementaci??n e integraci??n, adem??s de pruebas.

**La entrega ahora avanza por bloques utilizables.** El
[bloque 01 y el registro de entregas](entregas-incrementales.es.md) re??nen ocho programas
peque??os, un comando para compilarlos y ejecutarlos, circuitos OpenQASM e informes de recursos.
???10%, 20%?????? identificaba bloques del plan anterior; no representa cobertura de C11 ni esfuerzo
restante.

**Nuevo avance del bloque 06:** se implementan `strspn` y `strcspn` sobre vistas comprobadas, y
predicados directos `== NULL` / `!= NULL` para `memchr`, `strchr`, `strrchr`, `strpbrk` y `strstr`.
Tambi??n se a??aden posiciones mediante resta de punteros y comparaciones con direcciones
comprobadas del mismo objeto; `strrchr` selecciona la ??ltima coincidencia. Tambi??n se permite
guardar la b??squeda en un pr??stamo local inmutable, crear alias y leer caracteres con `*p` o
??ndices constantes. El selector se calcula una vez; el motor conserva l??mites demostrados de
resultados para justificar los consumidores posteriores. Los
[ejemplos y el contrato](c11-string-searches.md) describen el alcance. No hay todav??a un puntero
cu??ntico general devuelto por estas b??squedas. Por indicaci??n del usuario, las matrices
exhaustivas de estados, inversas y sanitizadores de este avance quedan para el cierre del bloque;
las campa??as anteriores no se presentan como validaci??n de este c??digo nuevo.

**Avance del bloque 02:** el [motor disperso](simulacion-dispersa.es.md) permite ejecutar circuitos
de hasta 128 qubits con soporte ocupado acotado. Los nuevos ejemplos usan realmente `int` de 32 bits
y `size_t` de 64 bits en el runtime C y miden expl??citamente al final; no solo exportan circuitos.
Se corrige adem??s un corte fijo de probabilidades peque??as en el motor denso. El bloque 02 y la
compatibilidad completa con C11 siguen abiertos.

El nuevo [selector de desplazamientos](c11-shifts.md) elimina los registros completos por etapa:
ahora generan circuito los desplazamientos variables de 32 bits que antes agotaban los recursos.
`variable_shift` ya ejecuta con 105 qubits en el modo disperso; el mismo programa falla por recursos
en la revisi??n anterior. Las pruebas dirigidas incluyen fases e inversas, conservando los l??mites
de las fuentes y dominios comprobados.

El nuevo `shift_predicate` usa desplazamientos de 64 bits como booleanos y tras convertirlos a byte,
con 85 qubits. Evita temporales enteros innecesarios y corrige una negaci??n l??gica incorrecta en
la ruta aritm??tica de expresiones anidadas. La
[evidencia](audit-evidence/shift-truth-v3/README.es.md) conserva los tres rechazos previos por
recursos, el error num??rico y los intentos de prueba fallidos.

**Nuevo c??digo integrado:** divisi??n reversible por constantes de magnitud 2 a 16 y consumo
directo del cociente como booleano, negaci??n y comparaci??n con cero. El ejemplo
[`quantum_buckets.c`](../examples/quantum_buckets.c) calcula grupos sobre una superposici??n de
bytes antes de medir. Las pruebas dirigidas normales y ASan/UBSan pasan 3505 comprobaciones de
expresiones, con 2310 dominios nuevos. La [descripci??n t??cnica](c11-division.md) distingue las
fuentes, los n??cleos reutilizados y el alcance de esas pruebas. La campa??a amplia anterior no
certifica autom??ticamente este nuevo c??digo.

Tambi??n se a??aden [`strcpy` y `strncpy`](c11-string-copy.md) hacia almacenamiento cu??ntico: copia
hasta NUL, conservaci??n de la cola y relleno con ceros seg??n corresponda. Las escrituras
reutilizan la historia reversible y admiten fuentes CPU conocidas. La calificaci??n completa sigue
aplazada; los dominios se comprueban dentro del circuito.

Tambi??n se incorpora [concatenaci??n con `strcat` y `strncat`](c11-string-concat.md): las
longitudes originales se capturan antes de escribir y las actualizaciones conservan historia
reversible. El ejemplo nuevo combina longitud de destino en superposici??n y append acotado desde
una fuente sin NUL. La calificaci??n completa sigue aplazada.

`strcat` y `strncat` tambi??n admiten ahora llamadas bajo un `if` cu??ntico mediante auxiliares
privados en cero. El ejemplo `controlled_concat` conserva los bytes del destino en la rama inactiva
e incluye una secuencia de fase y Fourier con resultado esperado `1`. Los contextos generales de
historia y las matrices completas de estados siguen pendientes de calificaci??n.

**Nuevo avance de memoria:** `memcpy` y `memmove` conservan historia al sobrescribir destinos
desconocidos y a??aden una ruta de bytes parciales, tama??os enteros diferentes y fuentes CPU
conocidas. El ejemplo `byte_copy` se ejecuta y exporta con el paquete del bloque 06; las matrices
completas de coherencia/inversa y contextos siguen aplazadas.

**Implementaci??n parcial:** se a??aden `atoi`, `atol` y `atoll` sobre cadenas cu??nticas
comprobadas, y escrituras de caracteres mediante punteros guardados de b??squeda, incluidos
asignaciones e incrementos con resultado. La campa??a de propiedad ahora ejecuta los ejemplos y
comprueba estados completos e inversas de consumidores elegidos. Los l??mites generales de cadenas y
dominios siguen pendientes; las campa??as anteriores conservan sus propias fuentes.

**Nuevo c??digo del bloque 04:** los arrays escalares fijos admiten ??ndices cu??nticos, lecturas y
escrituras con historia, asignaciones y valores de incremento/decremento. El rango se comprueba
antes de reducir el selector. Las formas generales de aritm??tica de punteros siguen pendientes.
Esta entrega tiene ejecuciones y pruebas finitas de estados e inversas, con integraci??n general
pendiente.

**Nuevo c??digo del bloque 03:** funciones encadenadas y pr??stamos de buffers admiten lecturas y
escrituras con ??ndice cu??ntico. Se recupera el ??ndice anterior al escalado de bytes para
comprobar su rango; tambi??n se corrige el rechazo de incrementos descartados envueltos en una
conversi??n entera de GCC. Los ejemplos de bytes y palabras ya ejecutan. Se agregan recorridos con
??ndice CPU, acumulaci??n y retorno de resultados. El [contrato de buffers](c11-buffer-functions.md)
mantiene expl??citos los contextos pendientes.

## La respuesta corta

El proyecto **ya compila y ejecuta programas con un subconjunto de operaciones cu??nticas sobre
enteros y booleanos**. Puede preparar superposiciones, conservar entrelazamiento y fases, construir
circuitos reversibles y realizar mediciones expl??citas. Tambi??n tiene adaptadores parciales para
algunas funciones de la biblioteca C.

**Todav??a no completa el n??cleo cotidiano acordado.** Hay trabajo de funciones, punteros, vectores
din??micos y combinaciones de flujo y memoria, adem??s de validaci??n. La compatibilidad cu??ntica
con toda la biblioteca C11 tambi??n es incompleta, pero queda fuera del criterio de cierre actual.

El c??digo C cl??sico se ejecuta en CPU mediante GCC y libc. Las operaciones cu??nticas se ejecutan
actualmente en un simulador de estados escrito en C. **La ciberseguridad permanece cl??sica y queda
fuera de la adaptaci??n cu??ntica**, de acuerdo con tu indicaci??n.

**La integraci??n anterior re??ne 151 entradas y reduce los auxiliares de las selecciones enteras
`condici??n ? s?? : no`.** Ya pasa la selecci??n directa entre `INT64_MIN` e `INT64_MAX` que antes
superaba los recursos. Se a??adieron 288 dominios finitos de regresi??n con todos los bytes,
controles de ambas polaridades, resultados de 8/32/64 bits, fases e inversas. Las suites dirigidas
pasan por configuraci??n normal y ASan/UBSan: 1195 expresiones, 55 desplazamientos, 990 divisiones,
393 nativas y 271 actualizaciones.

Otra prueba pas?? **768 estados complejos completos** de programas compilados, incluidos resultados
completos de 64 bits, selecciones anidadas y divisores CPU modificados despu??s de capturarlos. El
m??ximo observado en esas matrices es 102 qubits. La
[evidencia](audit-evidence/selection-views-v5/README.es.md) distingue las fuentes de las pruebas,
id??nticas en producci??n, del ajuste posterior del presupuesto del test. Conserva los errores de
codificaci??n y presupuesto de los primeros tests, y la procedencia de los n??cleos reutilizados y
verificados. La campa??a amplia actual reconstruye ambas configuraciones por separado; a??n no se
declara aprobada. Siguen pendientes expresiones generales, divisi??n com??n y el resto de C11.

**La integraci??n anterior re??ne 148 entradas y mejora el resto entero `%` con divisores de
magnitud 2 a 16.** El caso `int % 3` que antes exced??a los recursos pasa ahora la matriz finita de
tipos comunes: 640 capturas frente a 632 y cuatro l??mites anteriores. Se a??adieron 810 regresiones
de acci??n sobre bases por configuraci??n, con todos los bytes y l??mites elegidos de 32/64 bits,
promociones, bool, controles, fases e inversa. Pasaron reconstrucciones normales y con ASan/UBSan:
907 comprobaciones de expresiones, 990 de divisi??n, 393 nativas y 271 de actualizaciones cada una.

Otras pruebas de circuitos realmente compilados pasaron **1968 estados complejos completos** entre
ambas configuraciones, con referencias C, divisores CPU modificados despu??s de capturarlos y
limpieza por inversas. La [evidencia](audit-evidence/common-remainder-v2/README.es.md) distingue la
prueba anterior de 147 entradas, con fuentes de producci??n id??nticas, de la matriz actual de 148.
Conserva un fallo separado: preparar una selecci??n directa entre los extremos firmados de 64 bits
supera los 128 qubits antes del resto. La prueba posterior usa otra preparaci??n dentro del l??mite;
no elimina ese pendiente. La regresi??n amplia y el CI de estas 148 entradas siguen pendientes.

**La integraci??n anterior re??ne 146 entradas y corrige la limpieza de temporales conocidos de
`memset`.** Pasaron sus seis suites dirigidas normales y con ASan/UBSan: 940 comprobaciones de
llenado por configuraci??n, con 14 casos fuente nuevos. Otra prueba pas?? 3528 estados complejos
completos, con inversas emitidas por el compilador para los valores derivados y conservaci??n de
entradas desconocidas. Una reconstrucci??n normal de la ra??z Linux pas?? 20 ejecuciones exactas. La
[evidencia](audit-evidence/known-fill-cleanup-v2/README.es.md) conserva por separado los segmentos
interrumpidos, sus continuaciones y un l??mite de recursos de llenado parcial de 64 bits. La
regresi??n amplia y el CI de estas 146 entradas siguen pendientes.

**Ya hay una correcci??n integrada de operandos enteros bajo control cu??ntico.** La anterior
[integraci??n de 138 entradas](audit-evidence/controlled-operands-integration/integration-summary.json)
conserva el dominio seleccionado de asignaciones, sumas, XOR, los absolutos enteros est??ndar y
`memset`. El XOR booleano y los bucles con operandos literales o capturas escalares puras conservan
sus actualizaciones reversibles sobre el mismo registro, evitando dependencias y registros extra.

La [regresi??n amplia de esas 138 entradas](audit-evidence/controlled-operands-full/README.es.md)
termin?? correctamente en normal y ASan/UBSan: 76 pasos cerrados, 665 compilaciones de fuzzing por
configuraci??n y 70 circuitos independientes con QuEST por configuraci??n, 64 estados y 6 canales.
Las reconstrucciones separadas de integraci??n pasaron, por configuraci??n, 393 casos nativos, 1270
controles de operandos y 40 ejecuciones de los cuatro programas exactos del
[contrato](c11-controlled-operands.md). La aplicaci??n a Windows comprueba los hashes de las mismas
138 entradas; esa transferencia no equivale a ejecutar la regresi??n completa en Windows.

El [CI remoto de esta integraci??n](audit-evidence/integrated138-remote-ci/README.es.md) tambi??n
termin?? correctamente sobre el commit `08b9f3c`: formato y verificaci??n completa normal y con
ASan/UBSan. Se comprobaron las mismas 138 entradas del commit y los resultados de las 26 suites en
ambas configuraciones. Su fuzzing usa 145 compilaciones por configuraci??n y semilla 20261011; la
campa??a local ampliada conserva sus 665 compilaciones y semilla 20261012 por separado.

Las [pruebas independientes de v5](audit-evidence/controlled-operands-v5/README.es.md) conservan
8000 estados complejos e inversas de operandos, m??s 1200 estados y 600 ejecuciones nativas de XOR
booleano y bucles. Se conserva el fallo de coordinaci??n del primer controlador y la continuaci??n
instrumentada, sin inventar un c??digo de salida exitoso para el proceso original. Las dos
regresiones de v4 quedan como
[evidencia hist??rica](audit-evidence/controlled-operands-regression-failure/README.es.md).

**La integraci??n anterior de 144 entradas a??ade `strlen`, `strcmp`, ??ngulos diferidos de `phase`
y protecci??n de cargas CPU en ramas probadamente inactivas.** La
[evidencia actual](audit-evidence/strings-integration-v6/README.es.md) conserva los resultados
dirigidos normales y con ASan/UBSan: 460 comprobaciones de cadenas, 519 de bindings y 3380 de
??ngulos por configuraci??n, adem??s de las suites vecinas. El resultado de `strlen` conserva
`size_t`; `strcmp` usa bytes sin signo. Son adaptaciones parciales sobre almacenamiento entero
comprobado, con l??mites de tama??o, dominio y recursos.

Una prueba independiente de programas realmente compilados pas?? **640 estados complejos
completos**, antes y despu??s de sus inversas, entre normal y ASan/UBSan. Incluye resultados de 64
bits de `strlen`, resultados de 32 bits de `strcmp`, dos entradas cu??nticas y objetos CPU
modificados despu??s de capturarlos. Sus 320 capturas y 160 ejecuciones de referencias C son
distintas de las pruebas nativas. Los dos programas exactos del [contrato](c11-strings.md) pasaron
40 ejecuciones.

La nueva matriz de cargas CPU pas?? 140 filas, 420 ejecuciones nativas y 420 referencias C por
configuraci??n. Conserva por separado los
[fallos hist??ricos](audit-evidence/controlled-operands-v5/pending-cpu-loads.json). La consulta de
controles usa certificados simb??licos, sin medir ni inspeccionar amplitudes; sus
[76 pruebas originales](audit-evidence/guarded-cpu-loads-v3/README.es.md) mantienen su identidad de
fuentes. Otra prueba actual pas?? **1120 estados exportados de cargas CPU** con fases distintas por
entrada, historia retenida y limpieza por inversa del resultado derivado. La inversa de la propia
actualizaci??n controlada y la limpieza de su historia no se comprobaban en esa prueba. La nueva
matriz de 146 entradas a??ade inversas y limpieza para sus siete perfiles y dominios concretos; no
demuestra el contrato general de historia ni todos los contextos de vida de C11.

Tambi??n se conservan los
[siete fallos hist??ricos de las 137 entradas](audit-evidence/coherent-arm-operands/failure-summary.json).
Sus fuentes y resultados originales se mantienen separados de las correcciones posteriores.

**El CI remoto y la campa??a local ampliada de las 144 entradas terminaron correctamente.** La
[evidencia del CI](audit-evidence/integrated144-remote-ci/README.es.md) comprueba las 28 suites
normales y con ASan/UBSan del commit `45f71e7`, con las mismas fuentes del compilador. Incluye 145
compilaciones de fuzzing por configuraci??n y semilla 20261011. La campa??a local de 665
compilaciones por configuraci??n usa la semilla 20261012; termin?? en dos segmentos, con 80 comandos
correctos: 59 del prefijo cerrado y 21 de la continuaci??n. El
[cierre hist??rico](audit-evidence/common-remainder-v2/README.es.md) conserva los logs y el proceso
interrumpido sin inventarle una salida exitosa. Estos resultados de 144 entradas no certifican las
revisiones posteriores de 146 o 148. El
[fallo de bindings del candidato de fase](audit-evidence/deferred-phase-v6-broad-failure/README.es.md)
queda conservado: una conversi??n booleana heredaba el tipo `volatile` de la marca cu??ntica y se
confund??a con una lectura CPU con efectos. La correcci??n distingue los valores calculados de los
accesos reales; las nuevas pruebas siguen rechazando lecturas `volatile` e incrementos bajo control.

La correcci??n anterior de condiciones anidadas tuvo su propia integraci??n de 137 entradas. Su
[regresi??n local completa](audit-evidence/coherent-control-full/README.es.md) y su
[CI remoto](audit-evidence/integrated137-remote-ci/README.es.md) tambi??n terminaron bien. Los 145
casos compilados de fuzzing por configuraci??n del CI son distintos de las 665 compilaciones de la
campa??a local ampliada. Los resultados se conservan con sus fuentes y alcances originales.

## Qu?? significa que una operaci??n cu??ntica est?? implementada

Agregar `quant` o aceptar una llamada en el compilador no alcanza. Para los casos soportados, el
compilador debe construir una transformaci??n con la sem??ntica declarada: preservar los datos
necesarios, las fases y las correlaciones; conservar la informaci??n que haga falta para invertir el
c??lculo; y limpiar los temporales cuando corresponda.

Por ejemplo, calcular una comparaci??n puede producir un resultado asociado al dato de entrada sin
medirlo. Despu??s se puede usar ese resultado para aplicar una fase y deshacer la comparaci??n. La
medici??n aparece cuando el programa la pide expl??citamente. El reinicio tambi??n es una operaci??n
expl??cita y tiene un comportamiento distinto del c??lculo reversible.

Eso se comprueba con estados complejos completos, pruebas de inversas y circuitos exportados
ejecutados por un simulador independiente. **No se ha demostrado ejecuci??n en un procesador
cu??ntico f??sico ni aceleraci??n frente a una CPU.** Las pruebas son evidencia para sus casos y
dominios concretos; no demuestran ausencia de todos los bugs posibles.

## Qu?? est?? funcionando en el ??rbol principal

| ??rea                    | Qu?? est?? hecho                                                                                                                      | Alcance que hay que tener presente                                                                                                    |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| Compilaci??n             | Frontend real de GCC, ejecutables nativos, IR cu??ntica tipada, exportaci??n OpenQASM 3 e informes de recursos.                       | La configuraci??n de referencia es GCC 11.4.0 en Linux/WSL. No es una certificaci??n de todos los compiladores y plataformas.         |
| Valores cu??nticos       | Enteros y booleanos, preparaci??n de estados y superposiciones.                                                                       | Hay representaci??n de enteros hasta 64 bits, pero eso no garantiza que toda expresi??n de 64 bits entre en los recursos disponibles. |
| Operaciones              | Casos soportados de aritm??tica, operaciones bit a bit, desplazamientos y comparaciones.                                              | Las combinaciones, la divisi??n, los tipos con signo y los anchos grandes tienen condiciones y l??mites espec??ficos.                 |
| Fases y transformaciones | Fases, Fourier e inversa, condiciones cu??nticas coherentes y casos de b??squeda.                                                     | Las b??squedas tienen promesas o presupuestos declarados. No se afirma una soluci??n general sin restricciones.                       |
| Asignaciones             | Actualizaciones reversibles y parte de las asignaciones que necesitan conservar versiones anteriores.                                 | Sobrescribir informaci??n puede consumir registros adicionales. La historia desconocida no se borra silenciosamente.                  |
| Funciones y datos        | Funciones directas dentro de una misma unidad de compilaci??n, arrays fijos, campos de estructuras enteras y referencias comprobadas. | No equivale a soportar cualquier estructura, puntero, retorno o biblioteca cu??ntica enlazada por separado.                           |
| Control                  | Condiciones coherentes, bucles est??ticos acotados y ciertos bucles e ??ndices controlados por CPU.                                   | El flujo de control general y los ??ndices en superposici??n siguen incompletos.                                                      |
| Observaci??n             | Medici??n y reinicio expl??citos, con pruebas de sus efectos sobre estados correlacionados.                                           | Son operaciones distintas de una transformaci??n unitaria y se verifican por separado.                                                |

La [matriz t??cnica de cobertura](c-coverage.md) detalla las restricciones de cada construcci??n.

### Qu?? funciones de la biblioteca C tienen adaptaci??n parcial

| Familia                                    | Funciones                                                                                                                                                  | Qu?? falta para considerarlas soporte general                                                                                                                                                                                                                |
| ------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Clasificaci??n y conversi??n de caracteres | `isalnum`, `isalpha`, `isblank`, `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`, `isxdigit`, `tolower`, `toupper`. | Quedan contratos generales de llamadas indirectas, concurrencia, trazas de locale y otras combinaciones. Esto no implementa las funciones de caracteres anchos.                                                                                              |
| Valor absoluto entero                      | `abs`, `labs`, `llabs`, `imaxabs`.                                                                                                                         | Se conservan las condiciones de representabilidad, s??ntesis y recursos. No incluye valor absoluto flotante o complejo.                                                                                                                                      |
| Divisi??n entera con cociente y resto      | `div`, `ldiv`, `lldiv`, `imaxdiv`.                                                                                                                         | Hay restricciones sobre el dominio, la composici??n, los productos devueltos y los recursos.                                                                                                                                                                 |
| Copia y movimiento de memoria              | `memcpy`, `memmove`.                                                                                                                                       | Hay adaptadores para casos comprobados de almacenamiento entero. Faltan escrituras y layouts generales, memoria din??mica y otros contextos. No producen dos copias independientes de un estado cu??ntico desconocido.                                       |
| Llenado de memoria                         | `memset`.                                                                                                                                                  | Rangos fijos de almacenamiento entero, valor capturado una vez e historia de sobrescritura. Hay retornos locales comprobados; quedan consumidores generales, tama??os din??micos y layouts generales.                                                        |
| Comparaci??n de bytes y caracteres         | `memcmp`, `strncmp`, `strcmp`; longitud con `strlen`.                                                                                                      | Admite rangos comprobados, constantes y capturas ??nicas de bytes actuales de objetos CPU conocidos, conservadas para la inversa. Siguen pendientes las direcciones y tama??os din??micos, los accesos `volatile`/at??micos y otros contextos.               |
| Prefijos de cadenas                        | `strspn`, `strcspn`.                                                                                                                                       | Vistas de 1???64 bytes con NUL dentro del rango. La calificaci??n exhaustiva del nuevo avance queda pendiente.                                                                                                                                               |
| Predicados de b??squeda                    | `memchr`, `strchr`, `strrchr`, `strpbrk`, `strstr`.                                                                                                        | Comparaciones con NULL y direcciones del mismo objeto, adem??s de restas de posiciones. Se permiten pr??stamos locales inmutables, alias y lecturas de caracteres; escrituras, rebinding y ABI general siguen pendientes. Calificaci??n exhaustiva aplazada. |
| Copia de cadenas                           | `strcpy`, `strncpy`.                                                                                                                                       | Destino cu??ntico comprobado, cola preservada, padding e historia reversible; calificaci??n exhaustiva pendiente.                                                                                                                                            |
| Concatenaci??n                             | `strcat`, `strncat`.                                                                                                                                       | Longitudes originales capturadas antes de escribir, terminaci??n y cola preservada; calificaci??n exhaustiva pendiente.                                                                                                                                      |

Los heaps a??aden `malloc`, `calloc`, `realloc` y `free` con adaptaci??n parcial para capacidades
CPU, registros escalares y vida comprobada. Las operaciones de bytes/cadenas sobre esos heaps siguen
pendientes.

Son **47 nombres de funciones con adaptaci??n parcial**. Cada una conserva las restricciones de su
contrato; esta lista no dice que est??n implementadas para todos los programas C v??lidos.

Que `printf`, `malloc` u otra funci??n funcionen en la parte cl??sica de un programa no significa
que acepten directamente datos cu??nticos sin medir o que su sem??ntica cu??ntica est??
implementada.

## C??mo leer los n??meros del inventario

El [inventario legible por herramientas](c11-library.json) contiene **1224 entradas de 29 familias
de cabeceras**, con esta clasificaci??n:

| Clasificaci??n | Entradas | Qu?? significa                                                                                                |
| -------------- | -------: | ------------------------------------------------------------------------------------------------------------- |
| Pendientes     |      762 | Falta implementar o completar el an??lisis de su compatibilidad cu??ntica.                                    |
| Parciales      |       94 | Hay soporte para determinados casos, con restricciones y trabajo pendiente. Incluye funciones, tipos y alias. |
| Solo metadatos |      368 | Constantes, macros u otras facilidades que no representan una operaci??n cu??ntica de ejecuci??n.             |

El inventario no registra entradas como completamente implementadas. Las 94 parciales incluyen los
47 nombres de funciones anteriores y 47 entradas de tipos y alias; algunos nombres aparecen en m??s
de una cabecera. **No son 94 funciones completas.** El estado parcial registra c??digo disponible;
no certifica las pruebas aplazadas del nuevo bloque de cadenas.

Estos n??meros tampoco permiten calcular un porcentaje fiable del trabajo terminado: implementar un
tipo, una funci??n matem??tica y un administrador de memoria tiene costos muy diferentes.

## Qu?? se integr?? y qu?? permanece en pruebas

La ampliaci??n de `memcmp` y `strncmp` a **bytes actuales de objetos CPU conocidos ya est??
integrada**. Captura cada byte seleccionado una vez y conserva esos valores para deshacer el
c??lculo aunque la CPU vuelva a modificar el objeto. Las cargas de la referencia de `strncmp` se
deshabilitan despu??s de su propio NUL.

La auditor??a reprodujo una lectura despu??s del NUL junto a una p??gina inaccesible. Las primeras
versiones fallaban donde el C ordinario terminaba bien; la correcci??n protege la carga efectiva del
byte. Los fallos originales y sus fuentes quedaron conservados.

La campa??a completa normal y con ASan/UBSan termin?? en tramos, preservando la interrupci??n y la
continuaci??n sobre las mismas 129 fuentes. Incluye **650 comprobaciones de capturas y 63 circuitos
independientes: 57 estados y 6 casos con medici??n o reinicio**, adem??s de las regresiones previas.
En el ??rbol principal se reconstruyeron por separado las herramientas normales e instrumentadas y
pasaron las 650 comprobaciones y el ejemplo exacto del contrato en cada configuraci??n. La
regresi??n completa sigue identificada como una ejecuci??n aislada sobre fuentes id??nticas.

El [informe de integraci??n](re-audit-2026-10-17-byte-snapshots.md) y el
[contrato](c11-byte-snapshots.md) detallan los casos comprobados y sus l??mites. Esta ampliaci??n no
completa las dem??s funciones de strings ni elimina las restricciones de direcciones, tama??os,
memoria din??mica o accesos `volatile`/at??micos.

`memset` y la composici??n de llamadas de memoria **ya est??n integrados**. El llenado admite
escrituras parciales, arrays, alias y efectos cu??nticos secuenciados como `x++`, con captura ??nica
del valor e historia para los datos desconocidos sobrescritos. Las llamadas descartadas a `memset`,
`memcpy` y `memmove` pueden usarse en expresiones con coma y brazos seleccionados.

La regresi??n completa normal y con ASan/UBSan termin?? en tramos sobre las mismas **134 fuentes
congeladas**: incluye **800 comprobaciones de llenado, 360 de expresiones y 70 circuitos QuEST: 64
estados y 6 canales**. Su integraci??n original coincidi?? en todos esos hashes y tuvo
reconstrucciones normales e instrumentadas separadas: pasaron los dos ejemplos exactos de los
contratos en cinco niveles de optimizaci??n y con builtins activados y desactivados, 20 ejecuciones
por configuraci??n. Las regresiones completas siguen identificadas como ejecuciones del candidato
aislado. El [informe](re-audit-2026-10-17-memory-expressions.md) conserva esas distinciones y los
[contratos de llenado](c11-memset.md) y [expresiones](c11-memory-sequences.md) explican las
restricciones.

El proceso original se interrumpi?? durante las pruebas instrumentadas de biblioteca. Se conservaron
su prefijo y la continuaci??n, que repiti?? la suite incompleta entera y los controles restantes
sobre las mismas fuentes. No se invent?? un c??digo de salida para el proceso original.

Los retornos locales comprobados de `memcpy`, `memmove` y `memset`, las relaciones admitidas entre
direcciones y las distancias al mismo objeto siguen integrados. Los
[contratos](c11-memory-returns.md) describen sus l??mites. Las suites recurrentes pasaron 290 y 170
comprobaciones por configuraci??n en la campa??a completa anterior de 138 entradas. Los fallos
anteriores y las campa??as de 136 y 137 entradas conservan sus fuentes originales. Los consumidores
generales de punteros, sus tiempos de vida y la ABI general siguen incompletos.

## Orden de implementaci??n acordado

Primero se cierran los tipos habituales: `bool`, `char`, `short`, `int`, `long`, `long long`, sus
variantes sin signo, aliases, `size_t` y `ptrdiff_t`. Esto incluye conversiones, promociones,
operadores y limpieza de temporales; aceptar una declaraci??n no demuestra compatibilidad completa.

Despu??s siguen los contextos cotidianos: condiciones, bucles, funciones directas, arrays, structs
b??sicos, punteros y funciones frecuentes de memoria y cadenas. La correcci??n de vida de temporales
de `memset` ya tiene una integraci??n dirigida; su regresi??n amplia sigue pendiente. Los siguientes
tipos num??ricos ser??n `float` y `double`, junto con sus operaciones y conversiones, antes de
abordar `long double` y complejos.

Las interfaces menos habituales quedan en el roadmap ampliado. La aclaraci??n del 23 de octubre
prioriza cerrar programas cotidianos completos; no exige terminar todo C11. La ciberseguridad sigue
cl??sica. El [perfil vigente](perfil-c-cotidiano.es.md) detalla las prioridades y el cierre; no
representa una medici??n estad??stica de uso.

## Referencia de pendientes del roadmap ampliado

Esta tabla incluye tanto pendientes cotidianos como ampliaciones que no condicionan el objetivo
actual. Para distinguirlos, consultar el [perfil vigente](perfil-c-cotidiano.es.md).

| Bloque                                 | Trabajo pendiente principal                                                                                                                                                                        |
| -------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Flotantes y complejos                  | Representaci??n, operaciones y conversiones reversibles, redondeo, excepciones y casos especiales. Las amplitudes complejas del simulador no equivalen a tener implementado `_Complex` de C.       |
| Memoria din??mica y punteros generales | Asignaci??n y liberaci??n, tama??os y vidas din??micas, alias, procedencia, estructuras enlazadas y acceso coherente cuando el ??ndice es cu??ntico.                                               |
| Layouts generales                      | Uniones, bitfields, padding, productos y vistas de memoria m??s generales. El soporte actual de campos enteros fijos no cubre todos esos casos.                                                    |
| Funciones y enlace                     | ABI cu??ntica general, llamadas indirectas, recursi??n acotada, vari??dicas, retornos complejos y enlace de funciones C cu??nticas entre unidades de compilaci??n.                                 |
| Control, efectos e historia            | M??s combinaciones de operadores y memoria, salidas anticipadas, saltos, uniones de flujo y limpieza de valores hist??ricos en casos generales.                                                    |
| Biblioteca C                           | M??s funciones de strings, b??squeda, ordenamiento, matem??ticas, caracteres anchos y las dem??s interfaces pendientes, con integraci??n expl??cita de los efectos cl??sicos.                      |
| Cobertura y pruebas                    | Casos positivos, negativos, coherentes, inversas y combinaciones de los nuevos adaptadores; contrastes independientes y controles de recursos. Un rechazo esperado no cuenta como implementaci??n. |

Las operaciones con efectos cl??sicos, como archivos y salida por pantalla, necesitan una
integraci??n expl??cita con el programa cu??ntico. La medici??n debe aparecer donde corresponda; no
se pretende convertir autom??ticamente una escritura en disco en una puerta unitaria. **La
ciberseguridad contin??a en el lado cl??sico.**

La [hoja de ruta ampliada](c11-roadmap.md) conserva el desglose t??cnico. El objetivo cotidiano
incluye construcciones del lenguaje y su composici??n, adem??s de funciones de biblioteca.

## Qu?? est?? hecho en pruebas, papers y legibilidad

Hay pruebas de programas C completos, amplitudes, fases, inversas, canales expl??citos, recursos,
errores esperados y preservaci??n del comportamiento cl??sico. Las campa??as separan lo integrado,
lo probado en un candidato, lo exportado como circuito y lo simulado num??ricamente.

La
[integraci??n hist??rica de cadenas](audit-evidence/strings-integration-v6/integration-summary.json)
identifica 144 entradas, sus pruebas dirigidas cerradas y la regresi??n amplia todav??a pendiente.
La [campa??a completa anterior](audit-evidence/controlled-operands-full/summary.json) conserva sus
138 fuentes, dos reconstrucciones, cada comando cerrado y todos los tramos de fuzzing. Las campa??as
de 134 y 137 entradas tambi??n permanecen como resultados hist??ricos. No se renombran como pruebas
de las fuentes nuevas. Los
[informes originales de memoria](re-audit-2026-10-17-memory-expressions.md) conservan sus propias
reconstrucciones y sus interrupciones, con sus alcances declarados.

Los [informes de auditor??a de los papers](paper/evidence/paper-audit.json) registran verificaciones
autom??ticas por idioma, con 8 p??ginas, 7 figuras y 10 referencias por versi??n. Sus manifiestos
vinculan los documentos, las fuentes y la evidencia comprobada. Los experimentos hist??ricos del
paper conservan su revisi??n original y no se renombran como resultados del compilador nuevo. La
[revisi??n del 24 de octubre](paper/re-audit-2026-10-24.md) corrige una afirmaci??n falsa sobre el
umbral de enumeraci??n, protege las capturas hist??ricas y comprueba Bell y Grover con estados
complejos completos. Tambi??n vuelve a contrastar los estados e inversas archivados del heap. Este
cierre documental no equivale a terminar la implementaci??n del perfil cotidiano.

Se mejoraron la indentaci??n y el formato del c??digo y la documentaci??n. Las fuentes integradas
conservan el formato comprobado antes de congelarlas y los documentos actuales tienen su control
propio. Esto facilita la revisi??n; no constituye una prueba de ausencia de bugs.

## Qu?? permitir??a dar el objetivo por terminado

No alcanza con que pase el subconjunto actual de pruebas. Para cerrar el
[perfil cotidiano](perfil-c-cotidiano.es.md) habr??a que:

1. Resolver los pendientes del lenguaje y la biblioteca dentro del alcance acordado, manteniendo la
   ciberseguridad cl??sica. Cada incompatibilidad concreta necesita una explicaci??n y un
   diagn??stico preciso; no sirve llamar ??imposible?? a algo que simplemente falta implementar.
1. Completar y probar los contratos parciales y sus combinaciones, incluyendo sus l??mites de
   recursos y sus efectos sobre entradas, fases, historia y temporales.
1. Verificar la integraci??n de los cambios con pruebas normales, sanitizadores y referencias
   independientes apropiadas.
1. Dejar documentaci??n y papers consistentes con la implementaci??n y con las pruebas efectivamente
   realizadas, distinguiendo resultados actuales de experimentos hist??ricos.

El pr??ximo trabajo completa las combinaciones habituales de funciones, punteros, flujo y memoria.
Los vectores din??micos ya tienen un contrato nativo parcial, incluidos resultados de actualizaci??n
y copias heap???fijo dentro de bucles CPU. Faltan pruebas generales de direcciones disjuntas,
adaptadores de cadenas sobre heaps y m??s combinaciones de memoria y vida. Tambi??n siguen
pendientes los tipos cu??nticos `float` y `double` y la integraci??n general del perfil. Los papers
deber??n actualizarse si cambian los resultados que describen. Las campa??as dirigidas de propiedad
y heap conservan sus propias fuentes, ejecuciones y pruebas de estados e inversas. **El perfil
cotidiano todav??a no est?? completo.**

La [evidencia hist??rica de distancias](audit-evidence/pointer-differences/README.es.md) conserva
sus fuentes y resultados originales. La integraci??n posterior a??ade el uso positivo de una
distancia guardada en una variable cl??sica como condici??n bajo control cu??ntico, sin medici??n
impl??cita. Ese antiguo rechazo no se presenta como una restricci??n actual ni como prueba de que la
compatibilidad general de punteros est?? terminada.

La continuaci??n que termin?? y las reconstrucciones de integraci??n se ejecutaron en una copia
verificada dentro del sistema de archivos de Linux, sin montar unidades de Windows. La primera
continuaci??n tambi??n qued?? interrumpida, sin c??digo de salida conocido. Los archivos y
herramientas normales resultantes se aplican al repositorio de Windows mediante una transferencia
con controles de hashes de destino; esa copia no equivale a repetir toda la regresi??n en otra
ubicaci??n. La configuraci??n de aislamiento de WSL se conserva y la ciberseguridad sigue cl??sica.

Un reinicio adicional interrumpi?? fuzzing despu??s de 63 casos completos. La continuaci??n
conserv?? ese prefijo y us?? un controlador temporal con el mismo generador, programa y referencia
escalar. Se verificaron los 133 ??ndices originales, cinco niveles de optimizaci??n y los 256
valores de cada byte. Las 665 compilaciones instrumentadas son un resultado agregado de esos
segmentos, no una invocaci??n original ininterrumpida. Las fuentes adicionales del controlador y los
registros originales se conservan por separado.
