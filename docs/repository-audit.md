# Auditoría del repositorio y de los papers

Revisión del 12 de octubre de 2026. El objetivo final es una semántica cuántica explícita para todas
las construcciones e interfaces de C11 del perfil declarado. El compilador actual implementa un
subconjunto. Esta revisión identifica fallos reproducidos, corrige contratos y contrasta programas
finitos; no certifica C11 completo, todas las funciones de libc ni ausencia de cualquier bug.

La referencia histórica del paper es `1ebb2e308e61bed5632798ded5beecf77b6b9a9c`. La revisión actual
parte de `50ea73b8318490dd641c91913badba4aa35e79a3` e incluye cambios locales. El manifiesto
documental conserva hashes del código actual y distingue esos archivos de los logs históricos. No se
atribuyen las nuevas pruebas al commit del paper.

## Fallos reproducidos y corregidos

| Hallazgo                                                   | Reproducción y efecto anterior                                                                                     | Corrección y evidencia                                                                                                                                                                                    |
| ---------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Semántica clásica: cast alrededor de incremento/decremento | Con `n=255`, `(uint8_t)++n` producía 256 en vez de 0; el pase eliminaba también conversiones ordinarias de CPU     | La transformación especial se restringe a operandos cuánticos. Tres regresiones nativas y casos incluidos en cada programa del corpus de cinco niveles de optimización                                    |
| Préstamos `const` permitían efectos destructivos           | Un parámetro `quant const uint8_t *` podía llamar a `reset(*p)`; aliases locales perdían la calificación           | Se conserva el tipo del préstamo y se rechazan medición, reset, Fourier, transferencia y módulos que exigen almacenamiento mutable. Seis regresiones de composición                                       |
| Archivos de texto con NUL ocultaban bytes posteriores      | Un documento JSON válido seguido por NUL y basura era aceptado como su prefijo                                     | La lectura de texto rechaza NUL con `EILSEQ`; prueba binaria específica y validación negativa del backend                                                                                                 |
| Bucles de helpers clásicos contaminaban la IR cuántica     | Un helper con `for` impedía exportar un circuito estático independiente, mediante marcadores de control CPU ajenos | Los helpers ordinarios conservan el cuerpo de GCC. Pruebas de ejecución y exportación de la combinación clásica/cuántica                                                                                  |
| Efectos cuánticos de helpers sin contrato                  | La definición de un helper no utilizado podía añadir una operación a la IR global                                  | Se exige un procedimiento/parámetro cuántico declarado para esos efectos. Regresión negativa; los procedimientos cuánticos existentes siguen probados                                                     |
| Opciones necesarias no llegaban al enlace                  | `-fsanitize=undefined` compilaba C y fallaba al enlazar sus símbolos de runtime                                    | El driver conserva las opciones GCC en el enlace y el orden de bibliotecas después de los objetos. Regresión con UBSan                                                                                    |
| Contrato incorrecto en el header de `search`               | Prometía una cota para cualquier dominio no vacío, aunque podía no haber soluciones                                | La cota se condiciona a la existencia de soluciones y aleatoriedad uniforme independiente; no se garantiza por semilla. La derivación está en la hoja de ruta; los casos sin soluciones ya tienen pruebas |

El alcance de `const` es lectura del valor en la base y preservación del almacenamiento. Un control
de fase puede afectar amplitudes aunque preserve ese valor. No se presenta `const` como
inmutabilidad de todo el estado global.

## Implementación y contraste adicional

`abs`, `labs`, `llabs` e `imaxabs` tienen ahora adaptadores de valor entero que preservan el
argumento. Se conservan promociones y la distinción de GCC entre absoluto firmado y absoluto con
resultado sin signo; la operación IR firmada rechaza el resultado irrepresentable. Se genera
compute/copy/uncompute. Hay pruebas con builtins activados/desactivados, un helper homónimo que no
debe reescribirse, un dominio firmado pequeño completo, amplitudes e inversa, con y sin tablas. La
representación y los presupuestos siguen limitando casos más anchos; estas pruebas no cubren todos
los enteros de 64 bits. Se corrigieron además el caso válido `labs((long)INT_MIN)` con destino sin
signo y la pérdida del signo al estrechar un resultado de valor absoluto; ambos tienen regresiones.

QuEST contrasta además el valor absoluto con fase relativa y un programa que combina función, array,
bucle estático y control coherente. La exportación de los casos actuales se realiza con
`--no-tables`. Se comparan treinta estados y probabilidades completas, salvo una fase global, y tres
canales con 8192 transcripciones por implementación. El lector externo cubre el subconjunto QASM
emitido, no todo el lenguaje OpenQASM 3 ni dispositivos físicos.

`tests/c11_classical.c` incluye las 29 familias de headers y ejercita memoria, strings, formato,
conversiones, complejos, entorno flotante, atomics, hilos y saltos clásicos antes de una frontera
cuántica explícita. Se compara con GCC en cinco niveles, tanto normal como instrumentado. Es una
prueba de integración finita de la ruta clásica: no acredita adaptadores cuánticos para esas
familias.

## Avance de M2: desplazamientos variables

Se incorporó una red de desplazamiento controlado sin tablas, con conservación de operandos y
descomputación de sus etapas privadas. Respeta la anchura del operando izquierdo promovido para
validar el contador y conserva los bits altos antes de estrechar un resultado. El desplazamiento
derecho firmado utiliza la extensión de signo del perfil GCC declarado. No introduce medición ni
reset.

Las 55 comprobaciones adicionales incluyen veinte contrastes de estado conjunto en dominios pequeños
con fase, inversa, contadores correlacionados y control abierto; catorce rechazos de aritmética
indefinida; síntesis de ambos sentidos a 8/16 bits y agotamiento a 32; cinco ejecuciones combinadas
y diez exportaciones frontend a cinco niveles. Los casos grandes se identifican como síntesis, sin
atribuirles validación completa de estado. QuEST contrasta además dos casos de desplazamiento
variable con fase y conversión booleana.

La construcción retiene O(anchura × etapas del contador) de workspace. Sus casos de 32 bits exceden
el backend de 128 qubits: es una limitación de recursos de esta implementación, no incompatibilidad
física del operador. Los contratos, evidencia y pendientes se detallan en
[c11-shifts.md](c11-shifts.md). Las asignaciones destructivas, las expresiones con efectos y el
resto de M2 siguen abiertos; no se consideran completados por este avance.

## Avance de M2: expresiones enteras puras

Se incorporaron negación, complemento, operadores lógicos y selección condicional sin tablas. Las
pruebas de definición son locales a cada rama: una división solo necesita divisor no nulo donde se
selecciona, y una negación firmada excluye el mínimo representable. Se conservan tipos y
promociones, operandos, fases y una inversa del workspace privado; no se agregan mediciones ni
resets ocultos.

Se reprodujeron y corrigieron dos fallos adicionales. La interpretación del estado base de una IR
heredada podía perder el signo indicado por la expresión; una base de ocho bits con patrón 249 debe
significar -7 al leerla como firmada. La suma booleana de IR generaba paridad en la ruta sin tablas
en lugar de normalizar el resultado no nulo. Ambos casos tienen regresiones con y sin síntesis
finita.

Las 97 comprobaciones separan dominios conjuntos completos con fase e inversa, rechazos de
aritmética alcanzable indefinida, ejecuciones C combinadas en cinco niveles y generación ancha.
QuEST contrasta tres estados adicionales: división condicional protegida, cortocircuito con división
y negación/complemento condicionales. La prueba es conservadora; efectos secuenciados, historial y
predicados generales siguen abiertos. El contrato detallado está en
[c11-expressions.md](c11-expressions.md).

## Avance de M2: asignaciones reversibles sin tablas

Se reprodujo un fallo de política de síntesis: `--no-tables` todavía permitía enumerar una
permutación para algunas asignaciones destructivas. Esa ruta ahora exige habilitar tablas. Cuando no
existe una síntesis estructurada aplicable, el backend emite un diagnóstico y conserva la distinción
entre implementación pendiente, recursos e incompatibilidad de la transformación propuesta.

Las nuevas rutas certifican permutaciones de bits y transformaciones afines de palabras enteras con
multiplicador impar. Preservan promociones, definiciones de C y control coherente; no requieren
qubits privados, mediciones ni resets. La normalización booleana tiene un contrato separado: por
ejemplo, multiplicar un booleano por dos conserva su valor lógico, en lugar de calcular paridad. Las
asignaciones no inyectivas siguen necesitando conservación de historial o una extensión reversible
demostrada sobre el soporte permitido.

Las 271 comprobaciones adicionales incluyen estados conjuntos completos con fase e inversa,
controles abiertos y cerrados, política de tablas, rechazos definidos, generación a 16/32/64 bits y
programas C que combinan procedimientos, punteros clásicos a arrays, bucles y aritmética en cinco
niveles de optimización. QuEST contrasta tres estados adicionales. Para el caso reproducido
`uint8_t x; x *= 3`, la actualización pasó de 1045 puertas MCX a 28 puertas lógicas, sin ancillas;
estos recuentos no representan una descomposición física. El contrato, la demostración y los límites
de esta etapa se detallan en [c11-inplace.md](c11-inplace.md). El historial general y la cobertura
completa de M2 y C11 siguen pendientes.

## Avance de M4/M7: copias de memoria

`memcpy` y `memmove` tienen adaptadores parciales para registros enteros y arrays fijos con punteros
clásicos comprobados. Se preservan los bits de la representación del perfil, incluidos destinos
firmados. Un destino independiente en una base conocida se traduce reversiblemente antes de copiar;
un destino desconocido sigue necesitando historial. El resultado es una copia de valores de base que
puede crear correlaciones, no dos estados independientes clonados.

Se comprueban tamaño constante, límites, permisos y dependencias antes de escribir. `memcpy` rechaza
solapamiento; `memmove` ordena lecturas antes de escrituras y conserva los aliases exactos como
identidad. La transformación admite control coherente y no añade ancillas, mediciones ni resets. Los
wrappers fortificados de glibc se reconocen como interfaces del sistema, incluso con optimización y
builtins desactivados; las definiciones privadas homónimas conservan su semántica.

La nueva suite tiene 414 comprobaciones: 120 estados conjuntos con fase e inversa, cuatro estados de
solapamiento, 90 ejecuciones C, 170 rechazos de contrato y 30 generaciones anchas. QuEST contrasta
cuatro estados adicionales, incluido uno cuya fuente calculada se descomputa tras copiar. Los
contrastes focalizados y la campaña completa posterior, normal y ASan/UBSan, terminaron sin fallos.
Sus resultados finales y hashes se archivan con esta revisión.

Se detectó y cerró una frontera necesaria: copiar mediante XOR dentro de un bucle CPU podría
divergir de asignaciones C repetidas. Se exige una prueba por iteración; las expansiones estáticas
demostradas se comprueban individualmente. Layouts de bytes generales, padding, tamaños dinámicos,
historial y uso del puntero retornado siguen pendientes. El contrato detallado está en
[c11-memcopy.md](c11-memcopy.md); este avance no completa M4, M7 ni la biblioteca C11.

## Productos de división entera

Se reprodujo el rechazo de `div((int)n+5,2).quot`: el frontend no reconocía la proyección del
producto devuelto por libc, aunque el backend ya tenía división reversible. Ahora `div`, `ldiv`,
`lldiv` e `imaxdiv` producen campos `quot` y `rem` con sus tipos estándar. Se preservan los
argumentos, el redondeo hacia cero y el signo del resto. División por cero y `MIN/-1` se rechazan
incluso si solo se usa el resto; no se insertan mediciones ni resets.

Los divisores constantes de magnitud potencia de dos usan vistas de bits y correcciones firmadas sin
tablas. El programa con `div((int)n+5,2).quot` e `imaxdiv((intmax_t)n+5,2).rem`, proyectados a dos
destinos de ocho bits, genera un circuito con 20 qubits de pico. El producto completo tiene otro
tamaño de salida y exige contabilizar sus propios recursos. Los conteos describen circuitos lógicos;
la descomposición y conectividad del hardware requieren una evaluación adicional.

La suite [de división](c11-division.md) agrega 990 comprobaciones: 300 estados completos con fase e
inversa, 240 estados de las cuatro interfaces con control y denominador variable, 80 ejecuciones
numéricas firmadas, 40 llamadas clásicas con efectos de argumentos, 40 conversiones booleanas, diez
funciones privadas homónimas, 80 generaciones anchas, 40 contratos de producto completo y 160
diagnósticos negativos. Los tres circuitos QASM añadidos tienen contraste independiente con QuEST.
Las ejecuciones numéricas con medición/reset explícitos se distinguen de las pruebas coherentes sin
canales.

El ABI general de productos cuánticos y los argumentos cuánticos con efectos siguen pendientes
(**P**). Un producto con dos campos de 64 bits más una entrada viva exige al menos 129 qubits; el
límite actual de 128 se documenta y prueba como recursos (**R**), no como imposibilidad semántica.
El índice conserva 1224 interfaces: 368 de metadatos, 57 parciales y 799 pendientes. Ni ocho
entradas adicionales parciales ni esta campaña completan los hitos de C11.

## Captura CPU y preparación de productos clásicos

Se implementaron [parámetros CPU tipados](c11-bindings.md) para expresiones cuánticas, con captura
única de argumentos clásicos en las cuatro interfaces de división. La descomputación usa la
expresión concreta guardada, incluso si luego cambia la variable CPU original. Los valores se
conservan en memoria clásica durante la vida de la definición; no requieren medición, reset ni
registros cuánticos adicionales para transportar el parámetro.

La validación de un template sin valores es estructural. No certifica el dominio numérico, recursos
ni corrección de un circuito. El backend exige valores concretos para exportar; `--bindings` y las
APIs de especialización los incorporan antes de validar y sintetizar. Las filas faltantes,
duplicadas, desordenadas o incompatibles se rechazan antes de modificar el circuito.

También se corrigió un bug reproducido en la revisión publicada: una inicialización de array con dos
funciones que devolvían estructuras omitía ambas llamadas y preparaba cuatro ceros. La reproducción
mostró `calls=0; fields=0000`; la corrección muestra `calls=2; fields=1001`. Las estructuras
clásicas, incluidos retornos y miembros anidados, se capturan una vez y sus campos se preparan en la
base. No se introduce copia implícita de un estado cuántico.

Las 399 comprobaciones nuevas incluyen 96 estados completos con fase, controles e inversa guardada;
52 extremos tipados; 21 contratos negativos/de preparación; 190 casos C a cinco niveles y dos
configuraciones de builtins; y 40 estados completos de documentos C especializados. QuEST añade
división con parámetros CPU e inversa tras mutación clásica. Los efectos CPU en ramas coherentes y
el ABI general de retornos cuánticos siguen abiertos; esta etapa no completa C11.

La primera campaña con sanitizadores detectó un rechazo indebido de `n ? 1/d : false` con `n=false`
y `d=0`: el guard insertado por UBSan se clasificaba como un efecto CPU del programa. Se reconoce
ahora la instrumentación de división/desplazamiento en aritmética pura de ramas coherentes y se
delega su dominio al backend. Las regresiones contrastan división y desplazamiento inactivos,
rechazo de división activa por cero y callbacks CPU con punteros e incrementos dentro de un bucle.
Los efectos CPU reales y los accesos volátiles conservan sus restricciones.

## Sobrescrituras versionadas y orden de descomputación

Se implementó historial para asignaciones escalares incondicionales y aliases fijos comprobados. El
nuevo valor se calcula en almacenamiento limpio y la versión anterior conserva la información y las
dependencias antiguas. Los valores derivados se descomputan cuando terminan sus usos; las raíces en
una base conocida se restauran con puertas X; las raíces desconocidas permanecen en la salida
conjunta hasta el final del contexto. El reporte identifica qubits actuales y de pico del historial.
No se agrega medición, reset ni descarte parcial para hacerlo caber.

Se reprodujo un fallo de la revisión publicada `50ea73b`: medir `b=a*2`, con `a=(unsigned)n+3` y `n`
cuántico, podía programar la inversa de `a` antes de medir `b`. El backend rechazaba la ejecución
para proteger sus dependencias. El compilador ahora mantiene dichas dependencias hasta emitir la
medición o reset solicitado. La misma reproducción pasa y tiene regresión a cinco niveles de
optimización.

La revisión de la nueva selección de rutas también detectó una regresión: sumar registros distintos
se clasificaba como una expresión afín de una misma fuente, introduciendo historial innecesario. La
regla ahora exige una sola fuente; las pruebas existentes contrastan los 65 536 pares de suma sin
tablas ni workspace privado. Las combinaciones `x*2+1`, `x^(x>>1)` y `x*2+x` comprueban además la
selección de historial o actualización afín en la IR.

Las [207 comprobaciones de historial](c11-history.md) separan 78 dominios completos con fase e
inversa, cuatro contratos de vida/esquema, 75 integraciones C, 30 estados completos de documentos C
y 20 diagnósticos específicos. QuEST agrega tres circuitos sin tablas para limpieza derivada, salida
desconocida retenida e inversa con parámetros CPU capturados. Las uniones de ramas, las iteraciones
dinámicas, el manejo explícito del historial y la memoria general siguen pendientes P/R. Esta etapa
no completa las asignaciones de C11.

## Formato y legibilidad

Se normalizaron los archivos propios de C/C++, Python, Bash y documentación: cuatro espacios,
funciones y bloques desplegados, separación entre definiciones, prosa ajustada y ejemplos con el
mismo estilo. Los literales y el orden de los includes se conservan. El formateador verifica
tokens/directivas C y árboles sintácticos Python antes de escribir; la campaña de comportamiento
sigue siendo necesaria. GCC vuelve a advertir sobre sangría engañosa y CI comprueba el formato.

Las tres filas del inventario que contenían barras verticales de operadores se reescribieron para
evitar columnas rotas. Los manuscritos y PDFs conservan su composición auditada; los archivos
históricos y dependencias mantienen sus bytes. La guía reproducible está en
[CONTRIBUTING.md](../CONTRIBUTING.md).

## Reproducibilidad y estado de las campañas

El script `scripts/reproduce-paper-baseline.sh` reconstruyó el código histórico en un directorio
separado y confirmó 354 pruebas nativas, 426 comprobaciones de 16 ejemplos, 145 compilaciones
frontend, 24 circuitos sin tablas y 11 casos QuEST. El CSV de recursos se reprodujo exactamente. Los
logs publicados se conservaron. La reproducción exige GCC/G++ 11.4.0 y verifica la dependencia QuEST
fijada por revisión y checksum.

La campaña actual usa GCC/G++ 11.4.0, Ubuntu 22.04/WSL, semilla frontend 20261012, 128 casos
generados más cinco fijos y cinco niveles de optimización. Las semillas y tamaños no sustituyen una
prueba formal ni el análisis de los caminos no ejercitados. Los resultados finales de esta campaña
deben leerse junto con sus transcripciones y el manifiesto actual.

La campaña completa terminó sin fallos, normal y con ASan/UBSan: **389 pruebas nativas, 1452
comprobaciones de 36 ejemplos, 55 checks de desplazamientos, 97 de expresiones, 271 de asignaciones
reversibles, 414 de copias de memoria, 990 de división, 399 de parámetros CPU, 207 de historial, 18
de supervisión de validación, cinco comparaciones de integración clásica, 665 compilaciones
frontend, 24 circuitos de crecimiento y 33 casos QASM por configuración**. Incluye las correcciones
del absoluto sin signo y del signo anterior al estrechamiento, la red de desplazamientos variables,
expresiones condicionales puras, las regresiones de interpretación firmada y suma booleana, y la
política de tablas para actualizaciones reversibles, los adaptadores de memoria y división, y el
rechazo de fallos de infraestructura como pruebas negativas, la preparación de productos clásicos y
los guards puros de UBSan en ramas coherentes. Los treinta y tres casos externos corresponden a
treinta estados y tres canales; los once casos del paper siguen identificados como evidencia
histórica.

```bash
export CC=gcc-11 CXX=g++-11
export CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012
bash scripts/verify.sh
bash scripts/test-c11-classical.sh
python3 scripts/inventory_c11.py
bash scripts/reproduce-paper-baseline.sh
.deps/paper-venv/bin/python docs/paper/audit_paper.py
```

La transcripción final y el resumen con hashes de las fuentes se conservan en
[audit-evidence](audit-evidence/README.md). Se pueden archivar otra vez con
`python3 scripts/archive-audit.py --log build/audit/verify-history-full.log` después de reproducir
la campaña. El directorio local `build/audit/paper-baseline/` conserva la reconstrucción histórica.
Los metadatos describen el árbol de trabajo al archivar, antes de su publicación. Los hashes
permiten contrastarlo con la revisión que publique esos archivos; no se presenta como ejecución de
CI.

## Papers y garantías abiertas

Ambas ediciones mantienen ocho páginas, siete figuras originales, cuatro tablas y diez referencias.
Se inspeccionaron las 16 páginas renderizadas y se verificaron cifras, código, ecuaciones,
referencias, traducción y geometría. La redacción excluye notas internas y presenta C11 completo
como objetivo futuro; sus cifras siguen perteneciendo al estudio histórico de 0.4.0. La
[auditoría documental](paper/audit.md) precisa su alcance.

La [hoja de ruta](c11-roadmap.md), el [inventario de construcciones](c11-inventory.md) y el
[índice de interfaces](c11-library.json) dejan abiertos, entre otros, historial general para control
y memoria, expresiones completas con efectos, CFG y saltos generales, memoria
dinámica/VLA/unions/bitfields, ABI completa, flotantes/complejos cuánticos, biblioteca estándar,
concurrencia y direcciones coherentes. P indica implementación pendiente; R recursos o acotación; S
una incompatibilidad fundamentada en un contexto concreto. No se cierran entradas P mediante una
afirmación de imposibilidad física.

La amplitud del simulador `double complex` no es un dato C `_Complex` cuántico. Los circuitos
lógicos generados no prueban por sí solos corrección, ejecución en hardware, costos de
descomposición ni una cota global de error numérico. El análisis de GCC todavía confía en
transformaciones previas al pase; se contrasta una versión con programas finitos. La semántica
completa, más targets, validación externa ampliada y publicación de una revisión final verificable
siguen siendo criterios de cierre, no resultados de esta auditoría.

Se reprodujo y corrigió una debilidad del clasificador: timeout (124), fallo de ejecución (127) y un
reporte ASan sintético (1) satisfacían la antigua condición de rechazo. El runner ahora aborta ante
esas clases de infraestructura, señales, reportes de sanitizadores y errores internos del
compilador, incluso cuando la herramienta retorna cero. Rechaza NUL en texto y descarta los
fragmentos numerados de fuente de GCC al clasificar mensajes.

Dieciocho pruebas de procesos contrastan esas fronteras, normales y ASan/UBSan; los 170 negativos de
memoria requieren ahora su mensaje semántico correspondiente. Los negativos nativos utilizan el
mismo runner y exigen un diagnóstico C Ket con estado 1; algunos todavía necesitan mensajes
individuales más precisos. La [auditoría del runner](audit-validation.md) distingue fallos reales de
supervisión, reportes sintéticos y límites de detección. Esto fortalece la evidencia sin convertir
las pruebas finitas en una certificación de C11 completo.
