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

**La división anticipada en condiciones anidadas ya tiene una corrección integrada.** Las
reconstrucciones nuevas normales y con ASan/UBSan pasaron, por configuración, 393 pruebas nativas,
290 de retornos de memoria, 170 de distancias, 180 ejecuciones de condiciones y 30 diagnósticos. La
[integración](audit-evidence/coherent-memory-integration/integration-summary.json) conserva las 137
entradas exactas y distingue esos controles de la regresión amplia, que sigue en ejecución. Los 800
estados e inversas independientes pertenecen a la evidencia separada de la corrección.

**Siguen existiendo bugs confirmados:** ocho contrastes posteriores reprodujeron siete fallos de
operandos CPU dentro de ramas cuánticas inactivas: asignación, suma, XOR, `memset`, `abs` y `phase`.
El [registro](audit-evidence/coherent-arm-operands/failure-summary.json) conserva los programas, sus
referencias C exitosas y las señales de división por cero de los ejecutables aceptados a O2 con
builtins. El octavo caso, la condición anidada, sí pasó. También queda pendiente la lectura mediante
un puntero CPU inválido en una rama inactiva. La corrección tiene un alcance concreto; las
regresiones anteriores no demuestran ausencia de todos los bugs.

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
estados y 6 canales**. El árbol principal coincide en todos esos hashes y tuvo reconstrucciones
normales e instrumentadas separadas: pasaron los dos ejemplos exactos de los contratos en cinco
niveles de optimización y con builtins activados y desactivados, 20 ejecuciones por configuración.
Las regresiones completas siguen identificadas como ejecuciones del candidato aislado. El
[informe](re-audit-2026-10-17-memory-expressions.md) conserva esas distinciones y los
[contratos de llenado](c11-memset.md) y [expresiones](c11-memory-sequences.md) explican las
restricciones.

El proceso original se interrumpió durante las pruebas instrumentadas de biblioteca. Se conservaron
su prefijo y la continuación, que repitió la suite incompleta entera y los controles restantes sobre
las mismas fuentes. No se inventó un código de salida para el proceso original.

Los retornos locales comprobados de `memcpy`, `memmove` y `memset`, las relaciones admitidas entre
direcciones y las distancias al mismo objeto ya forman parte de la integración de 137 entradas. Los
[contratos](c11-memory-returns.md) describen los tipos, límites y consumidores admitidos. Las suites
recurrentes pasaron 290 y 170 comprobaciones por configuración en una reconstrucción nueva separada.
Los primeros fallos de expectativas antiguas se conservaron en su
[registro histórico](audit-evidence/memory-returns-regression-failure/failure-summary.json). Las
campañas anteriores de 136 y 137 entradas siguen separadas por sus fuentes. La regresión amplia de
la corrección actual continúa pendiente; los consumidores generales de punteros y la ABI general
siguen incompletos.

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

La [integración actual](audit-evidence/coherent-memory-integration/integration-summary.json)
identifica 137 entradas y pruebas específicas reconstruidas en normal y ASan/UBSan. Su regresión
amplia sigue pendiente. La
[campaña anterior de 134 entradas](audit-evidence/memory-expressions/integration-summary.json)
conserva su regresión completa y sus comprobaciones de integración originales. Las campañas
anteriores de [capturas de bytes](audit-evidence/byte-snapshots/integration-summary.json) y
[`strncmp`](audit-evidence/strncmp/integration-summary.json) conservan sus fuentes y resultados
históricos sin sobrescribirlos.

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

El próximo bloque de memoria es completar y verificar los consumidores de punteros devueltos, además
de los contratos generales de tamaños, layouts y efectos. Después continúa el trabajo por familias
pendientes. **Este lote ya integrado no completa C11 cuántico ni el objetivo general.**

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
