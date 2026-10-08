# Cobertura de C11 y datos `quant`

El objetivo de `|c>` es extender todo C11 con contratos cuánticos verificables. La implementación todavía no satisface ese objetivo completo. [1.0](v1-plan.md) exige un subconjunto cerrado y no requiere todo C11, QPU ni memoria dinámica. GCC analiza el lenguaje C; la tabla siguiente describe qué puede transformar el plugin cuando intervienen datos cuánticos. Reconocer una construcción con GCC no equivale a implementar su semántica cuántica.

Una operación que conserva información puede sintetizarse coherentemente. Una lectura hacia CPU requiere una medición explícita. Una escritura que pierde información necesita conservarla con propiedad y vida definidas, o un reset escrito por el programa. Rechazar casos todavía no implementados protege esos contratos, pero no se contabiliza como soporte funcional de la operación.

| Construcción de C11 | Estado sobre datos cuánticos | Contrato o trabajo pendiente |
|---|---|---|
| Preprocesador, includes y macros | GCC real | `quant` es un atributo registrado; todavía no un calificador nativo de GCC. |
| Enteros y `_Bool` | Implementado hasta 64 bits | Anchura, signo, promociones y conversiones conservados en IR. |
| `float`, `double`, complejos | Pendiente | Codificación, redondeo, excepciones y circuitos reversibles. Los ángulos CPU sí usan `double`. |
| `typedef` | Alias escalares compatibles | Propagación de `quant` como parte nativa del tipo pendiente. |
| `const` | Parámetros prestados de solo lectura | Propietarios constantes y préstamo general a través de toda la ABI pendientes. |
| `volatile`, `_Atomic`, hilos | C clásico; integración cuántica general pendiente | La marca `volatile` interna evita simplificaciones tempranas. El runtime cuántico es global. |
| Locales automáticas | Implementado | Propietarios en `main` y en procedimientos expandidos. |
| Globales, `static`, `extern`, almacenamiento TLS | Pendiente para propietarios cuánticos | Inicialización, sincronización, propiedad y limpieza entre llamadas. |
| Inicialización desde constante | Implementado | Prepara un estado de base. Una variable sin inicializador se prepara en cero. |
| Inicialización desde CPU | Implementado | Evalúa una vez y prepara almacenamiento nuevo; requiere ejecución nativa si el valor es dinámico. |
| Inicialización desde expresión cuántica | Implementado para expresiones admitidas | Conserva entradas, registra inversa y permite entrelazamiento. |
| Arrays fijos y multidimensionales | Implementado para enteros/bools | Propiedad y limpieza por elemento; expansión defensiva limitada. |
| Estructuras y estructuras anidadas | Implementado para productos enteros | Sin punteros internos, bitfields, unions ni campos anónimos cuánticos. |
| Inicializadores designados | Implementado en productos fijos | Elementos omitidos se preparan en cero. |
| Copia de arrays/estructuras completas | Pendiente | No puede heredar una copia independiente de un estado desconocido. |
| VLA y arrays de tamaño dinámico | Pendiente | Vida, límites y asignación de registros en ejecución. |
| Índices constantes | Implementado | Se comprueban los límites del almacenamiento. |
| Índices de bucles estáticos | Implementado | Se expanden bucles cuya cota y contador se comprueban. |
| Índices CPU dinámicos | Operaciones indexadas sobre arrays escalares y préstamos | Suma/resta/XOR e incrementos sin signo, medidas/reset y Fourier/inversa; límites comprobados. Lecturas cuánticas generales y productos anidados pendientes. |
| Índices cuánticos | Pendiente | Lectura/escritura coherente mediante selección reversible; sin lectura CPU implícita. |
| `&`, `*`, acceso mediante puntero | Préstamos comprobados | `quant T *p` referencia registros existentes; su dirección es clásica. Reinterpretar el tipo del elemento necesita un contrato de acceso por bytes y se rechaza. |
| Aritmética de punteros | Desplazamientos constantes y selección indexada por CPU | Se admite volver dentro de un array y representar un puntero a su final; accesos indexados comprueban límites en ejecución. Reasignación general de referencias pendiente. |
| Alias de referencias | Implementado en el alcance estático | Los alias apuntan al mismo registro; uso tras movimiento se rechaza. |
| Reasignación/escape de punteros | Pendiente | Las referencias actuales no pueden escapar a CPU ni reasignarse dinámicamente. |
| `malloc`, `free`, `realloc`, memoria cuántica dinámica | Pendiente | Gestor de almacenamiento, propiedad por dirección y limpieza antes de liberar. |
| Funciones y procedimientos directos | Composición sobre AST | Definición disponible antes de uso en la misma unidad; locales nuevos por llamada. |
| Parámetros escalares, punteros y arrays | Préstamos implementados | Igualdad de tipo C canónico; no consumir el préstamo ni solapar parámetros mutables. |
| Retorno cuántico | Expresiones puras condicionales y resultados escalares propios terminales | Un local puede transferirse a un inicializador nuevo; devolver un parámetro prestado directamente se rechaza. |
| Retornos generales y múltiples | Parcial | Retorno terminal propio y ramas puras; productos retornados y salidas tempranas con vida pendiente requieren análisis adicional. |
| Recursión, llamadas indirectas, funciones variádicas cuánticas | Pendiente | ABI y pruebas de vida/efectos y cotas. |
| Varias unidades de traducción y bibliotecas cuánticas | Módulos de circuitos QIR independientes | `circuit` e inversa embeben módulos tipados y unitarios; enlace de funciones C `quant` entre unidades pendiente. |
| `+ - * / %` en un resultado nuevo | Circuitos por registros y camino finito pequeño | Operandos positivos/negativos con rangos probados; divisor distinto de cero, overflow y MIN/-1 rechazados. Composiciones generales y divisor de 64 bits pendientes. |
| Suma/resta modular en el propio registro | Síntesis directa | Operandos CPU o registros sin signo distintos; conserva el otro registro. |
| `& | ^ ~` y desplazamientos constantes | Síntesis simbólica y camino finito | Límites de términos; validación de desplazamientos y conversiones. |
| `^=` | Acumulación reversible | Operando independiente del destino; argumento CPU evaluado una vez. Conversiones booleanas restringidas. |
| `*= /= %= &= |= <<= >>=` | Casos inyectivos en el camino finito | Transformaciones destructivas generales requieren conservar historial. |
| `++ --` | Contextos de actualización admitidos | El valor de una expresión con efectos cuánticos en cualquier contexto todavía no está implementado. |
| Comparaciones contra constantes | Camino estructurado o finito | Predicados de 16 bits probados exhaustivamente; ciertos casos de 32 bits exportables. |
| Comparaciones entre registros | Circuitos estructurados | Comparación léxica con diferencia reversible y sesgo del signo; limitada por espacio de trabajo. |
| `! && || ?:` puros | Implementado | Conserva cortocircuito en la evaluación de aritmética; no convierte efectos CPU en efectos coherentes. |
| Asignación escalar directa nueva | Movimiento | Transfiere propiedad, consume el nombre anterior y no asigna otra copia. |
| Asignación a un destino ocupado | Casos reversibles; general pendiente | Preservación explícita del historial y reglas para su destino. |
| `if/else` cuántico | Control coherente | Predicado calculado, control de ambas ramas e inversa; no mutar entradas necesarias para limpiarlo. |
| `if/else` CPU | Uniones conservadoras de estados | Conserva hechos ciertos en ambas ramas; creación dinámica y cambios de identidad pendientes. |
| `for` estático | Expansión acotada | Hasta 4096 iteraciones, sin saltos ni modificación del contador en el cuerpo. |
| `for/while/do` CPU dinámicos | Actualizaciones, `if` coherentes y medidas sobre propietarios estables | No se aplanan para QASM. Medir/resetear un elemento seleccionado no prueba que el bucle haya consumido todos los propietarios; asignación dinámica pendiente. |
| Bucles de condición cuántica | Pendiente | Cotas, avance reversible e historial. |
| `switch`, `break`, `continue`, `goto`, etiquetas | C clásico; composición cuántica general pendiente | Necesitan análisis del grafo de control, vidas y efectos. |
| `sizeof`, `_Alignof`, layout C clásico | GCC real | No leen amplitudes. El layout C no es una representación CPU del estado cuántico. |
| `read`, `scanf`, archivos y sockets | CPU clásico | Sus resultados pueden preparar nuevos registros; un estado cuántico debe medirse antes de enviarlo a CPU. |
| `write`, `printf`, archivos y sockets | CPU tras medida explícita | Se rechaza enviar un registro sin medir o efectuar I/O bajo control coherente. |
| `memcpy`, `memmove`, `memset` sobre registros | Pendiente | Acceso por bytes, propiedad y reversibilidad; no se simulan como copias CPU de los marcadores. |
| Comportamiento indefinido de C | Diagnósticos en el dominio sintetizado | No hay demostración completa de ausencia de UB en C arbitrario ni después de todas las transformaciones previas de GCC. |

## Transformaciones cuánticas adicionales

`phase(theta)` admite ángulos clásicos finitos, constantes o calculados en ejecución. `fourier` e `inverse_fourier` transforman todo un registro, con o sin controles coherentes, usando síntesis cuadrática. Ninguna necesita enumerar el dominio.

Los ejemplos `functions.c`, `vector_function.c`, `dynamic_vector.c`, `fourier.c` y `phase_estimation.c` prueban composición, préstamos, acceso por índice CPU y algoritmos con interferencia. `find_known` recibe la cantidad como promesa y `search` verifica candidatos con intentos acotados, incluso sin soluciones. `find` promete una solución. Ninguno cuenta previamente el dominio; el predicado todavía está sujeto a los límites de síntesis.

## Evidencia y criterio de avance

La suite comprueba programas C completos, amplitudes complejas, correlaciones, versiones de dependencias, limpieza e IR incorrecta. Las pruebas exhaustivas enumeran dominios concretos: 256 entradas de aritmética de ocho bits, 3720 pares de sumadores, 65.536 entradas de predicados y las bases de QFT de uno a seis bits. Hay además 1024 expresiones tipadas generadas con semilla fija, contrastadas con C sobre todas sus entradas e invertidas, y 4096 mutaciones de IR para ejercer parser y validación. No equivalen a cubrir todo C11 ni constituyen una campaña completa de fuzzing.

`scripts/sanitize.sh` instrumenta las pruebas y el motor. `scripts/sanitize-all.sh` construye además un plugin, driver, backend, runtime y ejecutables generados instrumentados, en `build/sanitized`. GCC anfitrión no está instrumentado; la detección de fugas se desactiva en sus procesos para no atribuir al plugin las reservas retenidas por GCC. ASan y UBSan continúan activos en el plugin; los checks escalares que GCC inserta antes de la pasada cuántica se sustituyen por la validación cuántica del dominio cuando corresponda. Los checks del código CPU se conservan.

Completar el lenguaje requiere cerrar las filas pendientes con implementaciones y pruebas de contratos, además de ampliar la síntesis. Esta tabla registra ese trabajo y no declara terminada la solicitud de cubrir todo C.

La nueva evidencia incluye amplitudes e inversas de los once operadores sin tablas, operandos negativos y redondeo C, crecimiento de 24 circuitos hasta 32 bits relevantes, frontend generado a cinco optimizaciones y once QASM importados directamente en Qiskit/Aer. Tres comparan transcripciones completas de medicion intermedia/reset. Los scripts y registros se describen en el README. La ABI de circuitos se detalla en [circuit-abi.md](circuit-abi.md).
