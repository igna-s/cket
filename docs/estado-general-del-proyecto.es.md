# Qué está hecho y qué falta en Quantum / `|c>`

**Estado actualizado el 24 de octubre de 2026.** Este documento resume el árbol principal y
distingue los cambios que todavía están en pruebas. Es una fotografía del estado actual; los
informes de cada campaña conservan sus resultados y fuentes originales.

**Longitudes CPU de memoria fija:** `memcpy`, `memmove` y `memset` seleccionan prefijos con tamaño
recibido en ejecución, dentro de objetos comprobados y hasta 64 bytes. El
[ejemplo](../examples/blocks/05/buffer_lengths.c) pasa la longitud de `argv` a una función. Se
comprueban límites y solapamiento antes de escribir. Se corrigen además una unión de ramas que
invalidaba un dueño del otro brazo y una lectura directa del retorno que podía obtener un
placeholder CPU. La lectura con `measure` usa el registro real; los accesos CPU sin observación se
rechazan. El [contrato](c11-runtime-memory.md) conserva los recursos y consumidores pendientes.

**Memoria fija dentro de bucles:** `memcpy`, `memmove` solapado y `memset` usan ahora historial por
iteración también entre buffers fijos. Un relleno que lee el destino se captura antes de modificar
la primera palabra. El [nuevo ejemplo](../examples/blocks/05/fixed_buffers.c) pasa un array a una
función y combina movimiento y relleno dentro de `while(rounds--)`, con 0 a 3 iteraciones. La
[campaña de memoria fija](audit-evidence/fixed-memory-loops-v1/README.es.md) registra 300 puntos de
estado completo, 90 inversas, 32 ejecuciones adicionales y las regresiones de copia/relleno, en
construcciones normales y ASan/UBSan. Ese cierre usaba tamaños y direcciones constantes; la
ampliación de longitudes CPU anterior conserva sus direcciones comprobadas.

**Nuevo cierre de bucles y funciones:** los resultados de `p[i]++`, las asignaciones escalares y las
copias heap→buffer fijo conservan ahora sus versiones en cada iteración CPU. También se corrige el
fallo interno al copiar a un parámetro de buffer dentro de una función con `while(n--)`; sus
argumentos CPU modificables tienen copias locales. El
[ejemplo de iteraciones](../examples/blocks/05/heap_iterations.c) combina incremento, acumulación y
copia. La [campaña de este bloque](audit-evidence/runtime-history-v1/README.es.md) registra 300
puntos de estado complejo completo, 90 inversas completas y 32 ejecuciones adicionales de contextos
nativos, con construcciones normales y ASan/UBSan. Son casos finitos; siguen pendientes la propiedad
en flujos generales, más consumidores de punteros, cadenas sobre heap y flotantes.

**Avance del bloque 05:** se implementan heaps cuánticos nativos con `malloc`, `calloc`, `realloc` y
`free`. El nuevo [programa dinámico](../examples/blocks/05/dynamic_heap.c) recibe la capacidad por
`argv`, reserva, crece, recorre mediante una función y libera. La carga sigue en registros
cuánticos; los punteros y capacidades son clásicos. Crecer conserva los registros; liberar no mide
ni borra estados desconocidos. El [contrato](c11-heap.md) detalla recursos y contextos pendientes.
La [campaña del heap](audit-evidence/heap-effects-v1/README.es.md) conserva los estados completos y
las inversas comprobadas. Se añaden escrituras con índice cuántico y resultados de asignación e
incremento que conservan sus versiones anteriores, incluso después de `free`. Es un avance parcial
del núcleo cotidiano.

**Nuevo bloque de memoria del heap:** `memcpy` y `memmove` entre heaps, y `memset` sobre heaps,
aceptan tamaños CPU de ejecución y vistas de bytes parciales. El nuevo
[ejemplo](../examples/blocks/05/heap_bytes.c) copia valores en superposición, mueve regiones
solapadas y rellena con un valor cuántico capturado. Se conservan las versiones originales y las
fases; los retornos son direcciones clásicas con alias comprobados. El [contrato](c11-heap.md)
mantiene explícitos los límites. Las cadenas sobre heap y los flotantes cuánticos siguen pendientes.
La [evidencia de este bloque](audit-evidence/heap-memory-v1/README.es.md) conserva 256 puntos de
estado completo, 64 inversiones completas de puertas y las comprobaciones con ASan/UBSan.

**Integración de buffers fijos y heap:** `memcpy` y `memmove` copian ahora en ambas direcciones, con
longitudes CPU y vistas fijas comprobadas. También se capturan bytes actuales de objetos CPU
conocidos hacia el heap. El [ejemplo](../examples/blocks/05/heap_transfer.c) conserva los resultados
después de cambiar el origen y liberar el heap. Las palabras fuera de la longitud conservan sus
qubits, incluida una copia de longitud cero. Las escrituras heap→fijo y los resultados de incremento
del heap añaden ahora [historia por iteración](c11-runtime-history.md). El nuevo
[ejemplo](../examples/blocks/05/heap_iterations.c) combina acumulación de postfix y copia dentro de
un bucle con longitud CPU, incluida longitud cero. Los usos generales de punteros y ABI siguen
abiertos. La [evidencia de integración](audit-evidence/heap-transfer-v1/README.es.md) registra 224
puntos de estado completo y 56 inversas, incluidas las construcciones con ASan/UBSan.

**Avance de propiedad y copia:** el usuario abrió esta etapa de validación. Se añaden lecturas de
buffers con índice CPU, acumulación/resta y paridad en bucles CPU, y resultados escalares devueltos
por funciones. La [campaña de propiedad](audit-evidence/ownership-contracts-v1/README.es.md) separa
ejecuciones, estados completos, fases, inversas y sanitizadores. Corrige además una suma que podía
ocultar un desbordamiento con signo al estrechar el resultado. Los
[contratos de no clonación](no-clonacion.es.md) explican qué significan copiar, prestar y mover.

**Alcance vigente:** completar el [C cotidiano funcional](perfil-c-cotidiano.es.md): tipos comunes,
funciones, control, arrays, punteros y vectores, con las operaciones de biblioteca que necesitan. El
usuario aclaró este alcance el 23 de octubre. El inventario completo de C11 queda como referencia
ampliada; completar todas sus entradas deja de ser requisito para este objetivo. El núcleo cotidiano
todavía tiene pendientes de implementación e integración, además de pruebas.

**La entrega ahora avanza por bloques utilizables.** El
[bloque 01 y el registro de entregas](entregas-incrementales.es.md) reúnen ocho programas pequeños,
un comando para compilarlos y ejecutarlos, circuitos OpenQASM e informes de recursos. “10%, 20%…”
identificaba bloques del plan anterior; no representa cobertura de C11 ni esfuerzo restante.

**Nuevo avance del bloque 06:** se implementan `strspn` y `strcspn` sobre vistas comprobadas, y
predicados directos `== NULL` / `!= NULL` para `memchr`, `strchr`, `strrchr`, `strpbrk` y `strstr`.
También se añaden posiciones mediante resta de punteros y comparaciones con direcciones comprobadas
del mismo objeto; `strrchr` selecciona la última coincidencia. También se permite guardar la
búsqueda en un préstamo local inmutable, crear alias y leer caracteres con `*p` o índices
constantes. El selector se calcula una vez; el motor conserva límites demostrados de resultados para
justificar los consumidores posteriores. Los [ejemplos y el contrato](c11-string-searches.md)
describen el alcance. No hay todavía un puntero cuántico general devuelto por estas búsquedas. Por
indicación del usuario, las matrices exhaustivas de estados, inversas y sanitizadores de este avance
quedan para el cierre del bloque; las campañas anteriores no se presentan como validación de este
código nuevo.

**Avance del bloque 02:** el [motor disperso](simulacion-dispersa.es.md) permite ejecutar circuitos
de hasta 128 qubits con soporte ocupado acotado. Los nuevos ejemplos usan realmente `int` de 32 bits
y `size_t` de 64 bits en el runtime C y miden explícitamente al final; no solo exportan circuitos.
Se corrige además un corte fijo de probabilidades pequeñas en el motor denso. El bloque 02 y la
compatibilidad completa con C11 siguen abiertos.

El nuevo [selector de desplazamientos](c11-shifts.md) elimina los registros completos por etapa:
ahora generan circuito los desplazamientos variables de 32 bits que antes agotaban los recursos.
`variable_shift` ya ejecuta con 105 qubits en el modo disperso; el mismo programa falla por recursos
en la revisión anterior. Las pruebas dirigidas incluyen fases e inversas, conservando los límites de
las fuentes y dominios comprobados.

El nuevo `shift_predicate` usa desplazamientos de 64 bits como booleanos y tras convertirlos a byte,
con 85 qubits. Evita temporales enteros innecesarios y corrige una negación lógica incorrecta en la
ruta aritmética de expresiones anidadas. La [evidencia](audit-evidence/shift-truth-v3/README.es.md)
conserva los tres rechazos previos por recursos, el error numérico y los intentos de prueba
fallidos.

**Nuevo código integrado:** división reversible por constantes de magnitud 2 a 16 y consumo directo
del cociente como booleano, negación y comparación con cero. El ejemplo
[`quantum_buckets.c`](../examples/quantum_buckets.c) calcula grupos sobre una superposición de bytes
antes de medir. Las pruebas dirigidas normales y ASan/UBSan pasan 3505 comprobaciones de
expresiones, con 2310 dominios nuevos. La [descripción técnica](c11-division.md) distingue las
fuentes, los núcleos reutilizados y el alcance de esas pruebas. La campaña amplia anterior no
certifica automáticamente este nuevo código.

También se añaden [`strcpy` y `strncpy`](c11-string-copy.md) hacia almacenamiento cuántico: copia
hasta NUL, conservación de la cola y relleno con ceros según corresponda. Las escrituras reutilizan
la historia reversible y admiten fuentes CPU conocidas. La calificación completa sigue aplazada; los
dominios se comprueban dentro del circuito.

También se incorpora [concatenación con `strcat` y `strncat`](c11-string-concat.md): las longitudes
originales se capturan antes de escribir y las actualizaciones conservan historia reversible. El
ejemplo nuevo combina longitud de destino en superposición y append acotado desde una fuente sin
NUL. La calificación completa sigue aplazada.

`strcat` y `strncat` también admiten ahora llamadas bajo un `if` cuántico mediante auxiliares
privados en cero. El ejemplo `controlled_concat` conserva los bytes del destino en la rama inactiva
e incluye una secuencia de fase y Fourier con resultado esperado `1`. Los contextos generales de
historia y las matrices completas de estados siguen pendientes de calificación.

**Nuevo avance de memoria:** `memcpy` y `memmove` conservan historia al sobrescribir destinos
desconocidos y añaden una ruta de bytes parciales, tamaños enteros diferentes y fuentes CPU
conocidas. El ejemplo `byte_copy` se ejecuta y exporta con el paquete del bloque 06; las matrices
completas de coherencia/inversa y contextos siguen aplazadas.

**Implementación parcial:** se añaden `atoi`, `atol` y `atoll` sobre cadenas cuánticas comprobadas,
y escrituras de caracteres mediante punteros guardados de búsqueda, incluidos asignaciones e
incrementos con resultado. La campaña de propiedad ahora ejecuta los ejemplos y comprueba estados
completos e inversas de consumidores elegidos. Los límites generales de cadenas y dominios siguen
pendientes; las campañas anteriores conservan sus propias fuentes.

**Nuevo código del bloque 04:** los arrays escalares fijos admiten índices cuánticos, lecturas y
escrituras con historia, asignaciones y valores de incremento/decremento. El rango se comprueba
antes de reducir el selector. Las formas generales de aritmética de punteros siguen pendientes. Esta
entrega tiene ejecuciones y pruebas finitas de estados e inversas, con integración general
pendiente.

**Nuevo código del bloque 03:** funciones encadenadas y préstamos de buffers admiten lecturas y
escrituras con índice cuántico. Se recupera el índice anterior al escalado de bytes para comprobar
su rango; también se corrige el rechazo de incrementos descartados envueltos en una conversión
entera de GCC. Los ejemplos de bytes y palabras ya ejecutan. Se agregan recorridos con índice CPU,
acumulación y retorno de resultados. El [contrato de buffers](c11-buffer-functions.md) mantiene
explícitos los contextos pendientes.

## La respuesta corta

El proyecto **ya compila y ejecuta programas con un subconjunto de operaciones cuánticas sobre
enteros y booleanos**. Puede preparar superposiciones, conservar entrelazamiento y fases, construir
circuitos reversibles y realizar mediciones explícitas. También tiene adaptadores parciales para
algunas funciones de la biblioteca C.

**Todavía no completa el núcleo cotidiano acordado.** Hay trabajo de funciones, punteros, vectores
dinámicos y combinaciones de flujo y memoria, además de validación. La compatibilidad cuántica con
toda la biblioteca C11 también es incompleta, pero queda fuera del criterio de cierre actual.

El código C clásico se ejecuta en CPU mediante GCC y libc. Las operaciones cuánticas se ejecutan
actualmente en un simulador de estados escrito en C. **La ciberseguridad permanece clásica y queda
fuera de la adaptación cuántica**, de acuerdo con tu indicación.

**La integración anterior reúne 151 entradas y reduce los auxiliares de las selecciones enteras
`condición ? sí : no`.** Ya pasa la selección directa entre `INT64_MIN` e `INT64_MAX` que antes
superaba los recursos. Se añadieron 288 dominios finitos de regresión con todos los bytes, controles
de ambas polaridades, resultados de 8/32/64 bits, fases e inversas. Las suites dirigidas pasan por
configuración normal y ASan/UBSan: 1195 expresiones, 55 desplazamientos, 990 divisiones, 393 nativas
y 271 actualizaciones.

Otra prueba pasó **768 estados complejos completos** de programas compilados, incluidos resultados
completos de 64 bits, selecciones anidadas y divisores CPU modificados después de capturarlos. El
máximo observado en esas matrices es 102 qubits. La
[evidencia](audit-evidence/selection-views-v5/README.es.md) distingue las fuentes de las pruebas,
idénticas en producción, del ajuste posterior del presupuesto del test. Conserva los errores de
codificación y presupuesto de los primeros tests, y la procedencia de los núcleos reutilizados y
verificados. La campaña amplia actual reconstruye ambas configuraciones por separado; aún no se
declara aprobada. Siguen pendientes expresiones generales, división común y el resto de C11.

**La integración anterior reúne 148 entradas y mejora el resto entero `%` con divisores de magnitud
2 a 16.** El caso `int % 3` que antes excedía los recursos pasa ahora la matriz finita de tipos
comunes: 640 capturas frente a 632 y cuatro límites anteriores. Se añadieron 810 regresiones de
acción sobre bases por configuración, con todos los bytes y límites elegidos de 32/64 bits,
promociones, bool, controles, fases e inversa. Pasaron reconstrucciones normales y con ASan/UBSan:
907 comprobaciones de expresiones, 990 de división, 393 nativas y 271 de actualizaciones cada una.

Otras pruebas de circuitos realmente compilados pasaron **1968 estados complejos completos** entre
ambas configuraciones, con referencias C, divisores CPU modificados después de capturarlos y
limpieza por inversas. La [evidencia](audit-evidence/common-remainder-v2/README.es.md) distingue la
prueba anterior de 147 entradas, con fuentes de producción idénticas, de la matriz actual de 148.
Conserva un fallo separado: preparar una selección directa entre los extremos firmados de 64 bits
supera los 128 qubits antes del resto. La prueba posterior usa otra preparación dentro del límite;
no elimina ese pendiente. La regresión amplia y el CI de estas 148 entradas siguen pendientes.

**La integración anterior reúne 146 entradas y corrige la limpieza de temporales conocidos de
`memset`.** Pasaron sus seis suites dirigidas normales y con ASan/UBSan: 940 comprobaciones de
llenado por configuración, con 14 casos fuente nuevos. Otra prueba pasó 3528 estados complejos
completos, con inversas emitidas por el compilador para los valores derivados y conservación de
entradas desconocidas. Una reconstrucción normal de la raíz Linux pasó 20 ejecuciones exactas. La
[evidencia](audit-evidence/known-fill-cleanup-v2/README.es.md) conserva por separado los segmentos
interrumpidos, sus continuaciones y un límite de recursos de llenado parcial de 64 bits. La
regresión amplia y el CI de estas 146 entradas siguen pendientes.

**Ya hay una corrección integrada de operandos enteros bajo control cuántico.** La anterior
[integración de 138 entradas](audit-evidence/controlled-operands-integration/integration-summary.json)
conserva el dominio seleccionado de asignaciones, sumas, XOR, los absolutos enteros estándar y
`memset`. El XOR booleano y los bucles con operandos literales o capturas escalares puras conservan
sus actualizaciones reversibles sobre el mismo registro, evitando dependencias y registros extra.

La [regresión amplia de esas 138 entradas](audit-evidence/controlled-operands-full/README.es.md)
terminó correctamente en normal y ASan/UBSan: 76 pasos cerrados, 665 compilaciones de fuzzing por
configuración y 70 circuitos independientes con QuEST por configuración, 64 estados y 6 canales. Las
reconstrucciones separadas de integración pasaron, por configuración, 393 casos nativos, 1270
controles de operandos y 40 ejecuciones de los cuatro programas exactos del
[contrato](c11-controlled-operands.md). La aplicación a Windows comprueba los hashes de las mismas
138 entradas; esa transferencia no equivale a ejecutar la regresión completa en Windows.

El [CI remoto de esta integración](audit-evidence/integrated138-remote-ci/README.es.md) también
terminó correctamente sobre el commit `08b9f3c`: formato y verificación completa normal y con
ASan/UBSan. Se comprobaron las mismas 138 entradas del commit y los resultados de las 26 suites en
ambas configuraciones. Su fuzzing usa 145 compilaciones por configuración y semilla 20261011; la
campaña local ampliada conserva sus 665 compilaciones y semilla 20261012 por separado.

Las [pruebas independientes de v5](audit-evidence/controlled-operands-v5/README.es.md) conservan
8000 estados complejos e inversas de operandos, más 1200 estados y 600 ejecuciones nativas de XOR
booleano y bucles. Se conserva el fallo de coordinación del primer controlador y la continuación
instrumentada, sin inventar un código de salida exitoso para el proceso original. Las dos
regresiones de v4 quedan como
[evidencia histórica](audit-evidence/controlled-operands-regression-failure/README.es.md).

**La integración anterior de 144 entradas añade `strlen`, `strcmp`, ángulos diferidos de `phase` y
protección de cargas CPU en ramas probadamente inactivas.** La
[evidencia actual](audit-evidence/strings-integration-v6/README.es.md) conserva los resultados
dirigidos normales y con ASan/UBSan: 460 comprobaciones de cadenas, 519 de bindings y 3380 de
ángulos por configuración, además de las suites vecinas. El resultado de `strlen` conserva `size_t`;
`strcmp` usa bytes sin signo. Son adaptaciones parciales sobre almacenamiento entero comprobado, con
límites de tamaño, dominio y recursos.

Una prueba independiente de programas realmente compilados pasó **640 estados complejos completos**,
antes y después de sus inversas, entre normal y ASan/UBSan. Incluye resultados de 64 bits de
`strlen`, resultados de 32 bits de `strcmp`, dos entradas cuánticas y objetos CPU modificados
después de capturarlos. Sus 320 capturas y 160 ejecuciones de referencias C son distintas de las
pruebas nativas. Los dos programas exactos del [contrato](c11-strings.md) pasaron 40 ejecuciones.

La nueva matriz de cargas CPU pasó 140 filas, 420 ejecuciones nativas y 420 referencias C por
configuración. Conserva por separado los
[fallos históricos](audit-evidence/controlled-operands-v5/pending-cpu-loads.json). La consulta de
controles usa certificados simbólicos, sin medir ni inspeccionar amplitudes; sus
[76 pruebas originales](audit-evidence/guarded-cpu-loads-v3/README.es.md) mantienen su identidad de
fuentes. Otra prueba actual pasó **1120 estados exportados de cargas CPU** con fases distintas por
entrada, historia retenida y limpieza por inversa del resultado derivado. La inversa de la propia
actualización controlada y la limpieza de su historia no se comprobaban en esa prueba. La nueva
matriz de 146 entradas añade inversas y limpieza para sus siete perfiles y dominios concretos; no
demuestra el contrato general de historia ni todos los contextos de vida de C11.

También se conservan los
[siete fallos históricos de las 137 entradas](audit-evidence/coherent-arm-operands/failure-summary.json).
Sus fuentes y resultados originales se mantienen separados de las correcciones posteriores.

**El CI remoto y la campaña local ampliada de las 144 entradas terminaron correctamente.** La
[evidencia del CI](audit-evidence/integrated144-remote-ci/README.es.md) comprueba las 28 suites
normales y con ASan/UBSan del commit `45f71e7`, con las mismas fuentes del compilador. Incluye 145
compilaciones de fuzzing por configuración y semilla 20261011. La campaña local de 665 compilaciones
por configuración usa la semilla 20261012; terminó en dos segmentos, con 80 comandos correctos: 59
del prefijo cerrado y 21 de la continuación. El
[cierre histórico](audit-evidence/common-remainder-v2/README.es.md) conserva los logs y el proceso
interrumpido sin inventarle una salida exitosa. Estos resultados de 144 entradas no certifican las
revisiones posteriores de 146 o 148. El
[fallo de bindings del candidato de fase](audit-evidence/deferred-phase-v6-broad-failure/README.es.md)
queda conservado: una conversión booleana heredaba el tipo `volatile` de la marca cuántica y se
confundía con una lectura CPU con efectos. La corrección distingue los valores calculados de los
accesos reales; las nuevas pruebas siguen rechazando lecturas `volatile` e incrementos bajo control.

La corrección anterior de condiciones anidadas tuvo su propia integración de 137 entradas. Su
[regresión local completa](audit-evidence/coherent-control-full/README.es.md) y su
[CI remoto](audit-evidence/integrated137-remote-ci/README.es.md) también terminaron bien. Los 145
casos compilados de fuzzing por configuración del CI son distintos de las 665 compilaciones de la
campaña local ampliada. Los resultados se conservan con sus fuentes y alcances originales.

## Qué significa que una operación cuántica esté implementada

Agregar `quant` o aceptar una llamada en el compilador no alcanza. Para los casos soportados, el
compilador debe construir una transformación con la semántica declarada: preservar los datos
necesarios, las fases y las correlaciones; conservar la información que haga falta para invertir el
cálculo; y limpiar los temporales cuando corresponda.

Por ejemplo, calcular una comparación puede producir un resultado asociado al dato de entrada sin
medirlo. Después se puede usar ese resultado para aplicar una fase y deshacer la comparación. La
medición aparece cuando el programa la pide explícitamente. El reinicio también es una operación
explícita y tiene un comportamiento distinto del cálculo reversible.

Eso se comprueba con estados complejos completos, pruebas de inversas y circuitos exportados
ejecutados por un simulador independiente. **No se ha demostrado ejecución en un procesador cuántico
físico ni aceleración frente a una CPU.** Las pruebas son evidencia para sus casos y dominios
concretos; no demuestran ausencia de todos los bugs posibles.

## Qué está funcionando en el árbol principal

| Área                     | Qué está hecho                                                                                                                       | Alcance que hay que tener presente                                                                                                  |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| Compilación              | Frontend real de GCC, ejecutables nativos, IR cuántica tipada, exportación OpenQASM 3 e informes de recursos.                        | La configuración de referencia es GCC 11.4.0 en Linux/WSL. No es una certificación de todos los compiladores y plataformas.         |
| Valores cuánticos        | Enteros y booleanos, preparación de estados y superposiciones.                                                                       | Hay representación de enteros hasta 64 bits, pero eso no garantiza que toda expresión de 64 bits entre en los recursos disponibles. |
| Operaciones              | Casos soportados de aritmética, operaciones bit a bit, desplazamientos y comparaciones.                                              | Las combinaciones, la división, los tipos con signo y los anchos grandes tienen condiciones y límites específicos.                  |
| Fases y transformaciones | Fases, Fourier e inversa, condiciones cuánticas coherentes y casos de búsqueda.                                                      | Las búsquedas tienen promesas o presupuestos declarados. No se afirma una solución general sin restricciones.                       |
| Asignaciones             | Actualizaciones reversibles y parte de las asignaciones que necesitan conservar versiones anteriores.                                | Sobrescribir información puede consumir registros adicionales. La historia desconocida no se borra silenciosamente.                 |
| Funciones y datos        | Funciones directas dentro de una misma unidad de compilación, arrays fijos, campos de estructuras enteras y referencias comprobadas. | No equivale a soportar cualquier estructura, puntero, retorno o biblioteca cuántica enlazada por separado.                          |
| Control                  | Condiciones coherentes, bucles estáticos acotados y ciertos bucles e índices controlados por CPU.                                    | El flujo de control general y los índices en superposición siguen incompletos.                                                      |
| Observación              | Medición y reinicio explícitos, con pruebas de sus efectos sobre estados correlacionados.                                            | Son operaciones distintas de una transformación unitaria y se verifican por separado.                                               |

La [matriz técnica de cobertura](c-coverage.md) detalla las restricciones de cada construcción.

### Qué funciones de la biblioteca C tienen adaptación parcial

| Familia                                  | Funciones                                                                                                                                                  | Qué falta para considerarlas soporte general                                                                                                                                                                                                              |
| ---------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Clasificación y conversión de caracteres | `isalnum`, `isalpha`, `isblank`, `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`, `isxdigit`, `tolower`, `toupper`. | Quedan contratos generales de llamadas indirectas, concurrencia, trazas de locale y otras combinaciones. Esto no implementa las funciones de caracteres anchos.                                                                                           |
| Valor absoluto entero                    | `abs`, `labs`, `llabs`, `imaxabs`.                                                                                                                         | Se conservan las condiciones de representabilidad, síntesis y recursos. No incluye valor absoluto flotante o complejo.                                                                                                                                    |
| División entera con cociente y resto     | `div`, `ldiv`, `lldiv`, `imaxdiv`.                                                                                                                         | Hay restricciones sobre el dominio, la composición, los productos devueltos y los recursos.                                                                                                                                                               |
| Copia y movimiento de memoria            | `memcpy`, `memmove`.                                                                                                                                       | Hay adaptadores para casos comprobados de almacenamiento entero. Faltan escrituras y layouts generales, memoria dinámica y otros contextos. No producen dos copias independientes de un estado cuántico desconocido.                                      |
| Llenado de memoria                       | `memset`.                                                                                                                                                  | Rangos fijos de almacenamiento entero, valor capturado una vez e historia de sobrescritura. Hay retornos locales comprobados; quedan consumidores generales, tamaños dinámicos y layouts generales.                                                       |
| Comparación de bytes y caracteres        | `memcmp`, `strncmp`, `strcmp`; longitud con `strlen`.                                                                                                      | Admite rangos comprobados, constantes y capturas únicas de bytes actuales de objetos CPU conocidos, conservadas para la inversa. Siguen pendientes las direcciones y tamaños dinámicos, los accesos `volatile`/atómicos y otros contextos.                |
| Prefijos de cadenas                      | `strspn`, `strcspn`.                                                                                                                                       | Vistas de 1–64 bytes con NUL dentro del rango. La calificación exhaustiva del nuevo avance queda pendiente.                                                                                                                                               |
| Predicados de búsqueda                   | `memchr`, `strchr`, `strrchr`, `strpbrk`, `strstr`.                                                                                                        | Comparaciones con NULL y direcciones del mismo objeto, además de restas de posiciones. Se permiten préstamos locales inmutables, alias y lecturas de caracteres; escrituras, rebinding y ABI general siguen pendientes. Calificación exhaustiva aplazada. |
| Copia de cadenas                         | `strcpy`, `strncpy`.                                                                                                                                       | Destino cuántico comprobado, cola preservada, padding e historia reversible; calificación exhaustiva pendiente.                                                                                                                                           |
| Concatenación                            | `strcat`, `strncat`.                                                                                                                                       | Longitudes originales capturadas antes de escribir, terminación y cola preservada; calificación exhaustiva pendiente.                                                                                                                                     |

Los heaps añaden `malloc`, `calloc`, `realloc` y `free` con adaptación parcial para capacidades CPU,
registros escalares y vida comprobada. Las operaciones de bytes/cadenas sobre esos heaps siguen
pendientes.

Son **47 nombres de funciones con adaptación parcial**. Cada una conserva las restricciones de su
contrato; esta lista no dice que estén implementadas para todos los programas C válidos.

Que `printf`, `malloc` u otra función funcionen en la parte clásica de un programa no significa que
acepten directamente datos cuánticos sin medir o que su semántica cuántica esté implementada.

## Cómo leer los números del inventario

El [inventario legible por herramientas](c11-library.json) contiene **1224 entradas de 29 familias
de cabeceras**, con esta clasificación:

| Clasificación  | Entradas | Qué significa                                                                                                 |
| -------------- | -------: | ------------------------------------------------------------------------------------------------------------- |
| Pendientes     |      762 | Falta implementar o completar el análisis de su compatibilidad cuántica.                                      |
| Parciales      |       94 | Hay soporte para determinados casos, con restricciones y trabajo pendiente. Incluye funciones, tipos y alias. |
| Solo metadatos |      368 | Constantes, macros u otras facilidades que no representan una operación cuántica de ejecución.                |

El inventario no registra entradas como completamente implementadas. Las 94 parciales incluyen los
47 nombres de funciones anteriores y 47 entradas de tipos y alias; algunos nombres aparecen en más
de una cabecera. **No son 94 funciones completas.** El estado parcial registra código disponible; no
certifica las pruebas aplazadas del nuevo bloque de cadenas.

Estos números tampoco permiten calcular un porcentaje fiable del trabajo terminado: implementar un
tipo, una función matemática y un administrador de memoria tiene costos muy diferentes.

## Qué se integró y qué permanece en pruebas

La ampliación de `memcmp` y `strncmp` a **bytes actuales de objetos CPU conocidos ya está
integrada**. Captura cada byte seleccionado una vez y conserva esos valores para deshacer el cálculo
aunque la CPU vuelva a modificar el objeto. Las cargas de la referencia de `strncmp` se deshabilitan
después de su propio NUL.

La auditoría reprodujo una lectura después del NUL junto a una página inaccesible. Las primeras
versiones fallaban donde el C ordinario terminaba bien; la corrección protege la carga efectiva del
byte. Los fallos originales y sus fuentes quedaron conservados.

La campaña completa normal y con ASan/UBSan terminó en tramos, preservando la interrupción y la
continuación sobre las mismas 129 fuentes. Incluye **650 comprobaciones de capturas y 63 circuitos
independientes: 57 estados y 6 casos con medición o reinicio**, además de las regresiones previas.
En el árbol principal se reconstruyeron por separado las herramientas normales e instrumentadas y
pasaron las 650 comprobaciones y el ejemplo exacto del contrato en cada configuración. La regresión
completa sigue identificada como una ejecución aislada sobre fuentes idénticas.

El [informe de integración](re-audit-2026-10-17-byte-snapshots.md) y el
[contrato](c11-byte-snapshots.md) detallan los casos comprobados y sus límites. Esta ampliación no
completa las demás funciones de strings ni elimina las restricciones de direcciones, tamaños,
memoria dinámica o accesos `volatile`/atómicos.

`memset` y la composición de llamadas de memoria **ya están integrados**. El llenado admite
escrituras parciales, arrays, alias y efectos cuánticos secuenciados como `x++`, con captura única
del valor e historia para los datos desconocidos sobrescritos. Las llamadas descartadas a `memset`,
`memcpy` y `memmove` pueden usarse en expresiones con coma y brazos seleccionados.

La regresión completa normal y con ASan/UBSan terminó en tramos sobre las mismas **134 fuentes
congeladas**: incluye **800 comprobaciones de llenado, 360 de expresiones y 70 circuitos QuEST: 64
estados y 6 canales**. Su integración original coincidió en todos esos hashes y tuvo
reconstrucciones normales e instrumentadas separadas: pasaron los dos ejemplos exactos de los
contratos en cinco niveles de optimización y con builtins activados y desactivados, 20 ejecuciones
por configuración. Las regresiones completas siguen identificadas como ejecuciones del candidato
aislado. El [informe](re-audit-2026-10-17-memory-expressions.md) conserva esas distinciones y los
[contratos de llenado](c11-memset.md) y [expresiones](c11-memory-sequences.md) explican las
restricciones.

El proceso original se interrumpió durante las pruebas instrumentadas de biblioteca. Se conservaron
su prefijo y la continuación, que repitió la suite incompleta entera y los controles restantes sobre
las mismas fuentes. No se inventó un código de salida para el proceso original.

Los retornos locales comprobados de `memcpy`, `memmove` y `memset`, las relaciones admitidas entre
direcciones y las distancias al mismo objeto siguen integrados. Los
[contratos](c11-memory-returns.md) describen sus límites. Las suites recurrentes pasaron 290 y 170
comprobaciones por configuración en la campaña completa anterior de 138 entradas. Los fallos
anteriores y las campañas de 136 y 137 entradas conservan sus fuentes originales. Los consumidores
generales de punteros, sus tiempos de vida y la ABI general siguen incompletos.

## Orden de implementación acordado

Primero se cierran los tipos habituales: `bool`, `char`, `short`, `int`, `long`, `long long`, sus
variantes sin signo, aliases, `size_t` y `ptrdiff_t`. Esto incluye conversiones, promociones,
operadores y limpieza de temporales; aceptar una declaración no demuestra compatibilidad completa.

Después siguen los contextos cotidianos: condiciones, bucles, funciones directas, arrays, structs
básicos, punteros y funciones frecuentes de memoria y cadenas. La corrección de vida de temporales
de `memset` ya tiene una integración dirigida; su regresión amplia sigue pendiente. Los siguientes
tipos numéricos serán `float` y `double`, junto con sus operaciones y conversiones, antes de abordar
`long double` y complejos.

Las interfaces menos habituales quedan en el roadmap ampliado. La aclaración del 23 de octubre
prioriza cerrar programas cotidianos completos; no exige terminar todo C11. La ciberseguridad sigue
clásica. El [perfil vigente](perfil-c-cotidiano.es.md) detalla las prioridades y el cierre; no
representa una medición estadística de uso.

## Referencia de pendientes del roadmap ampliado

Esta tabla incluye tanto pendientes cotidianos como ampliaciones que no condicionan el objetivo
actual. Para distinguirlos, consultar el [perfil vigente](perfil-c-cotidiano.es.md).

| Bloque                                | Trabajo pendiente principal                                                                                                                                                                       |
| ------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Flotantes y complejos                 | Representación, operaciones y conversiones reversibles, redondeo, excepciones y casos especiales. Las amplitudes complejas del simulador no equivalen a tener implementado `_Complex` de C.       |
| Memoria dinámica y punteros generales | Asignación y liberación, tamaños y vidas dinámicas, alias, procedencia, estructuras enlazadas y acceso coherente cuando el índice es cuántico.                                                    |
| Layouts generales                     | Uniones, bitfields, padding, productos y vistas de memoria más generales. El soporte actual de campos enteros fijos no cubre todos esos casos.                                                    |
| Funciones y enlace                    | ABI cuántica general, llamadas indirectas, recursión acotada, variádicas, retornos complejos y enlace de funciones C cuánticas entre unidades de compilación.                                     |
| Control, efectos e historia           | Más combinaciones de operadores y memoria, salidas anticipadas, saltos, uniones de flujo y limpieza de valores históricos en casos generales.                                                     |
| Biblioteca C                          | Más funciones de strings, búsqueda, ordenamiento, matemáticas, caracteres anchos y las demás interfaces pendientes, con integración explícita de los efectos clásicos.                            |
| Cobertura y pruebas                   | Casos positivos, negativos, coherentes, inversas y combinaciones de los nuevos adaptadores; contrastes independientes y controles de recursos. Un rechazo esperado no cuenta como implementación. |

Las operaciones con efectos clásicos, como archivos y salida por pantalla, necesitan una integración
explícita con el programa cuántico. La medición debe aparecer donde corresponda; no se pretende
convertir automáticamente una escritura en disco en una puerta unitaria. **La ciberseguridad
continúa en el lado clásico.**

La [hoja de ruta ampliada](c11-roadmap.md) conserva el desglose técnico. El objetivo cotidiano
incluye construcciones del lenguaje y su composición, además de funciones de biblioteca.

## Qué está hecho en pruebas, papers y legibilidad

Hay pruebas de programas C completos, amplitudes, fases, inversas, canales explícitos, recursos,
errores esperados y preservación del comportamiento clásico. Las campañas separan lo integrado, lo
probado en un candidato, lo exportado como circuito y lo simulado numéricamente.

La
[integración histórica de cadenas](audit-evidence/strings-integration-v6/integration-summary.json)
identifica 144 entradas, sus pruebas dirigidas cerradas y la regresión amplia todavía pendiente. La
[campaña completa anterior](audit-evidence/controlled-operands-full/summary.json) conserva sus 138
fuentes, dos reconstrucciones, cada comando cerrado y todos los tramos de fuzzing. Las campañas de
134 y 137 entradas también permanecen como resultados históricos. No se renombran como pruebas de
las fuentes nuevas. Los [informes originales de memoria](re-audit-2026-10-17-memory-expressions.md)
conservan sus propias reconstrucciones y sus interrupciones, con sus alcances declarados.

Los [informes de auditoría de los papers](paper/evidence/paper-audit.json) registran verificaciones
automáticas por idioma, con 8 páginas, 7 figuras y 10 referencias por versión. Sus manifiestos
vinculan los documentos, las fuentes y la evidencia comprobada. Los experimentos históricos del
paper conservan su revisión original y no se renombran como resultados del compilador nuevo. La
[revisión del 24 de octubre](paper/re-audit-2026-10-24.md) corrige una afirmación falsa sobre el
umbral de enumeración, protege las capturas históricas y comprueba Bell y Grover con estados
complejos completos. También vuelve a contrastar los estados e inversas archivados del heap. Este
cierre documental no equivale a terminar la implementación del perfil cotidiano.

Se mejoraron la indentación y el formato del código y la documentación. Las fuentes integradas
conservan el formato comprobado antes de congelarlas y los documentos actuales tienen su control
propio. Esto facilita la revisión; no constituye una prueba de ausencia de bugs.

## Qué permitiría dar el objetivo por terminado

No alcanza con que pase el subconjunto actual de pruebas. Para cerrar el
[perfil cotidiano](perfil-c-cotidiano.es.md) habría que:

1. Resolver los pendientes del lenguaje y la biblioteca dentro del alcance acordado, manteniendo la
   ciberseguridad clásica. Cada incompatibilidad concreta necesita una explicación y un diagnóstico
   preciso; no sirve llamar «imposible» a algo que simplemente falta implementar.
1. Completar y probar los contratos parciales y sus combinaciones, incluyendo sus límites de
   recursos y sus efectos sobre entradas, fases, historia y temporales.
1. Verificar la integración de los cambios con pruebas normales, sanitizadores y referencias
   independientes apropiadas.
1. Dejar documentación y papers consistentes con la implementación y con las pruebas efectivamente
   realizadas, distinguiendo resultados actuales de experimentos históricos.

El próximo trabajo completa las combinaciones habituales de funciones, punteros, flujo y memoria.
Los vectores dinámicos ya tienen un contrato nativo parcial, incluidos resultados de actualización y
copias heap→fijo dentro de bucles CPU. Faltan pruebas generales de direcciones disjuntas,
adaptadores de cadenas sobre heaps y más combinaciones de memoria y vida. También siguen pendientes
los tipos cuánticos `float` y `double` y la integración general del perfil. Los papers deberán
actualizarse si cambian los resultados que describen. Las campañas dirigidas de propiedad y heap
conservan sus propias fuentes, ejecuciones y pruebas de estados e inversas. **El perfil cotidiano
todavía no está completo.**

La [evidencia histórica de distancias](audit-evidence/pointer-differences/README.es.md) conserva sus
fuentes y resultados originales. La integración posterior añade el uso positivo de una distancia
guardada en una variable clásica como condición bajo control cuántico, sin medición implícita. Ese
antiguo rechazo no se presenta como una restricción actual ni como prueba de que la compatibilidad
general de punteros esté terminada.

La continuación que terminó y las reconstrucciones de integración se ejecutaron en una copia
verificada dentro del sistema de archivos de Linux, sin montar unidades de Windows. La primera
continuación también quedó interrumpida, sin código de salida conocido. Los archivos y herramientas
normales resultantes se aplican al repositorio de Windows mediante una transferencia con controles
de hashes de destino; esa copia no equivale a repetir toda la regresión en otra ubicación. La
configuración de aislamiento de WSL se conserva y la ciberseguridad sigue clásica.

Un reinicio adicional interrumpió fuzzing después de 63 casos completos. La continuación conservó
ese prefijo y usó un controlador temporal con el mismo generador, programa y referencia escalar. Se
verificaron los 133 índices originales, cinco niveles de optimización y los 256 valores de cada
byte. Las 665 compilaciones instrumentadas son un resultado agregado de esos segmentos, no una
invocación original ininterrumpida. Las fuentes adicionales del controlador y los registros
originales se conservan por separado.
