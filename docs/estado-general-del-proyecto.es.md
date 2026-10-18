# Qué está hecho y qué falta en Quantum / `|c>`

**Estado actualizado el 18 de octubre de 2026.** Este documento resume el árbol principal y
distingue los cambios que todavía están en pruebas. Es una fotografía del estado actual; los
informes de cada campaña conservan sus resultados y fuentes originales.

## La respuesta corta

El proyecto **ya compila y ejecuta programas con un subconjunto de operaciones cuánticas sobre
enteros y booleanos**. Puede preparar superposiciones, conservar entrelazamiento y fases, construir
circuitos reversibles y realizar mediciones explícitas. También tiene adaptadores parciales para
algunas funciones de la biblioteca C.

**Todavía no implementa compatibilidad cuántica completa con C11 ni con toda su biblioteca.** Hay
trabajo de implementación importante por delante, además de pruebas y documentación.

El código C clásico se ejecuta en CPU mediante GCC y libc. Las operaciones cuánticas se ejecutan
actualmente en un simulador de estados escrito en C. **La ciberseguridad permanece clásica y queda
fuera de la adaptación cuántica**, de acuerdo con tu indicación.

**Ya hay una corrección integrada de operandos enteros bajo control cuántico.** La
[integración de 138 entradas](audit-evidence/controlled-operands-integration/integration-summary.json)
conserva el dominio seleccionado de asignaciones, sumas, XOR, los absolutos enteros estándar y
`memset`. El XOR booleano y los bucles con operandos literales o capturas escalares puras conservan
sus actualizaciones reversibles sobre el mismo registro, evitando dependencias y registros extra.

La [regresión amplia](audit-evidence/controlled-operands-full/README.es.md) terminó correctamente en
normal y ASan/UBSan: 76 pasos cerrados, 665 compilaciones de fuzzing por configuración y 70
circuitos independientes con QuEST por configuración, 64 estados y 6 canales. Las reconstrucciones
separadas de integración pasaron, por configuración, 393 casos nativos, 1270 controles de operandos
y 40 ejecuciones de los cuatro programas exactos del [contrato](c11-controlled-operands.md). La
aplicación a Windows comprueba los hashes de las mismas 138 entradas; esa transferencia no equivale
a ejecutar la regresión completa en Windows.

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

**Siguen existiendo bugs confirmados.** El registro anterior de 137 entradas conserva
[siete fallos](audit-evidence/coherent-arm-operands/failure-summary.json): seis perfiles enteros
tienen correcciones y pruebas positivas en la integración nueva; el ángulo de `phase` sigue
pendiente. La [matriz de cargas CPU](audit-evidence/controlled-operands-v5/pending-cpu-loads.json)
conserva siete perfiles, cinco niveles de optimización y ambos modos de builtins: 140 ejecutables
cuánticos aceptados fallan, mientras las 70 referencias C terminan bien. Incluye asignación, XOR,
suma, `memset`, `phase`, condiciones e índices. La integración actual sigue afectada por esa
familia.

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

| Familia                                  | Funciones                                                                                                                                                  | Qué falta para considerarlas soporte general                                                                                                                                                                                               |
| ---------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Clasificación y conversión de caracteres | `isalnum`, `isalpha`, `isblank`, `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`, `isxdigit`, `tolower`, `toupper`. | Quedan contratos generales de llamadas indirectas, concurrencia, trazas de locale y otras combinaciones. Esto no implementa las funciones de caracteres anchos.                                                                            |
| Valor absoluto entero                    | `abs`, `labs`, `llabs`, `imaxabs`.                                                                                                                         | Se conservan las condiciones de representabilidad, síntesis y recursos. No incluye valor absoluto flotante o complejo.                                                                                                                     |
| División entera con cociente y resto     | `div`, `ldiv`, `lldiv`, `imaxdiv`.                                                                                                                         | Hay restricciones sobre el dominio, la composición, los productos devueltos y los recursos.                                                                                                                                                |
| Copia y movimiento de memoria            | `memcpy`, `memmove`.                                                                                                                                       | Hay adaptadores para casos comprobados de almacenamiento entero. Faltan escrituras y layouts generales, memoria dinámica y otros contextos. No producen dos copias independientes de un estado cuántico desconocido.                       |
| Llenado de memoria                       | `memset`.                                                                                                                                                  | Rangos fijos de almacenamiento entero, valor capturado una vez e historia de sobrescritura. Hay retornos locales comprobados; quedan consumidores generales, tamaños dinámicos y layouts generales.                                        |
| Comparación de bytes y caracteres        | `memcmp`, `strncmp`.                                                                                                                                       | Admite rangos comprobados, constantes y capturas únicas de bytes actuales de objetos CPU conocidos, conservadas para la inversa. Siguen pendientes las direcciones y tamaños dinámicos, los accesos `volatile`/atómicos y otros contextos. |

Son **27 nombres de funciones con adaptación parcial**. Cada una conserva las restricciones de su
contrato; esta lista no dice que estén implementadas para todos los programas C válidos.

Que `printf`, `malloc` u otra función funcionen en la parte clásica de un programa no significa que
acepten directamente datos cuánticos sin medir o que su semántica cuántica esté implementada.

## Cómo leer los números del inventario

El [inventario legible por herramientas](c11-library.json) contiene **1224 entradas de 29 familias
de cabeceras**, con esta clasificación:

| Clasificación  | Entradas | Qué significa                                                                                                 |
| -------------- | -------: | ------------------------------------------------------------------------------------------------------------- |
| Pendientes     |      782 | Falta implementar o completar el análisis de su compatibilidad cuántica.                                      |
| Parciales      |       74 | Hay soporte para determinados casos, con restricciones y trabajo pendiente. Incluye funciones, tipos y alias. |
| Solo metadatos |      368 | Constantes, macros u otras facilidades que no representan una operación cuántica de ejecución.                |

El inventario no registra entradas como completamente implementadas. Las 74 parciales incluyen los
27 nombres de funciones anteriores y 47 entradas de tipos y alias; algunos nombres aparecen en más
de una cabecera. **No son 74 funciones completas.**

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
comprobaciones por configuración en la campaña completa de las 138 entradas actuales. Los fallos
anteriores y las campañas de 136 y 137 entradas conservan sus fuentes originales. Los consumidores
generales de punteros, sus tiempos de vida y la ABI general siguen incompletos.

## Qué falta implementar para el objetivo grande

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

La [hoja de ruta](c11-roadmap.md) contiene el desglose técnico. El objetivo incluye las
construcciones del lenguaje, no solamente los nombres de funciones del inventario.

## Qué está hecho en pruebas, papers y legibilidad

Hay pruebas de programas C completos, amplitudes, fases, inversas, canales explícitos, recursos,
errores esperados y preservación del comportamiento clásico. Las campañas separan lo integrado, lo
probado en un candidato, lo exportado como circuito y lo simulado numéricamente.

La [integración actual](audit-evidence/controlled-operands-integration/integration-summary.json)
identifica las 138 entradas y sus pruebas separadas de instalación. La
[campaña completa de esas fuentes](audit-evidence/controlled-operands-full/summary.json) conserva
sus dos reconstrucciones, cada comando cerrado y todos los tramos de fuzzing. Las campañas de 134 y
137 entradas permanecen como resultados históricos; no se renombran como pruebas de las fuentes
nuevas. Los [informes originales de memoria](re-audit-2026-10-17-memory-expressions.md) conservan
sus propias reconstrucciones y sus interrupciones, con sus alcances declarados.

Los [informes de auditoría de los papers](paper/evidence/paper-audit.json) registran verificaciones
automáticas por idioma, con 8 páginas, 7 figuras y 10 referencias por versión. Sus manifiestos
vinculan los documentos, las fuentes y la evidencia comprobada. Los experimentos históricos del
paper conservan su revisión original y no se renombran como resultados del compilador nuevo.

Se mejoraron la indentación y el formato del código y la documentación. Las fuentes integradas
conservan el formato comprobado antes de congelarlas y los documentos actuales tienen su control
propio. Esto facilita la revisión; no constituye una prueba de ausencia de bugs.

## Qué permitiría dar el objetivo por terminado

No alcanza con que pase el subconjunto actual de pruebas. Para cerrar el objetivo habría que:

1. Resolver los pendientes del lenguaje y la biblioteca dentro del alcance acordado, manteniendo la
   ciberseguridad clásica. Cada incompatibilidad concreta necesita una explicación y un diagnóstico
   preciso; no sirve llamar «imposible» a algo que simplemente falta implementar.
1. Completar y probar los contratos parciales y sus combinaciones, incluyendo sus límites de
   recursos y sus efectos sobre entradas, fases, historia y temporales.
1. Verificar la integración de los cambios con pruebas normales, sanitizadores y referencias
   independientes apropiadas.
1. Dejar documentación y papers consistentes con la implementación y con las pruebas efectivamente
   realizadas, distinguiendo resultados actuales de experimentos históricos.

El próximo paso concreto es corregir los ángulos de `phase` y las cargas CPU que todavía se evalúan
en ramas cuánticas inactivas. También siguen pendientes los consumidores generales de punteros,
tamaños, layouts, efectos y las demás familias de C11. **La integración nueva no completa C11
cuántico ni el objetivo general.**

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
