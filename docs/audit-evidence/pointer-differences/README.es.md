# Resta de punteros: resultado del experimento

Este cambio está en un **prototipo aislado**. Todavía no forma parte del compilador principal ni
demuestra compatibilidad completa con los punteros o con la biblioteca C11.

El compilador ahora puede resolver la distancia entre dos referencias comprobadas al mismo objeto
cuántico. Usa información de las direcciones; no lee ni mide el estado cuántico para calcularla. Por
ejemplo, para un puntero a elementos de 16 bits, `(p + 1) - p` debe producir `1`, mientras que la
diferencia entre esas direcciones convertidas a punteros de bytes debe producir `2`.

La implementación conserva esa distinción: GCC representa primero la diferencia en bytes y añade la
división por el tamaño del elemento. Dividir dos veces daría un resultado incorrecto.

## Qué pasó las pruebas

| Comprobación                                  | Resultado | Alcance                                                                |
| --------------------------------------------- | --------: | ---------------------------------------------------------------------- |
| Ejecución de programas admitidos              |        80 | Ocho casos, cinco niveles de optimización y dos modos de builtins.     |
| Rechazos de casos fuera del contrato          |        40 | Cuatro casos con las mismas diez configuraciones.                      |
| Repetición con ASan y UBSan                   |       120 | Plugin nuevo instrumentado y ejecutables generados instrumentados.     |
| Referencias en C ordinario                    |        40 | Se cuentan aparte de las ejecuciones cuánticas.                        |
| Estados complejos e inversas                  |        40 | Un perfil coherente, diez configuraciones y dos políticas de síntesis. |
| Igualdad con circuitos de distancia constante |        40 | Se comparan los circuitos efectivamente exportados.                    |

Los estados independientes comprueban amplitudes, fases, correlaciones, historia conservada y
limpieza de temporales después de invertir el cálculo. El perfil recorre dos amplitudes sobre
circuitos de nueve o diez qubits; este resultado no se presenta como una prueba de todos los estados
posibles.

Los componentes del motor, runtime y controlador que no cambiaron se reutilizaron y se identifican
por sus hashes. Los plugins normales e instrumentados se reconstruyeron. Esta campaña dirigida no
equivale a reconstruir y ejecutar toda la regresión del repositorio con la nueva extensión.

## Qué sigue pendiente

Un programa válido que guarda la distancia en una variable clásica y luego usa esa variable como
condición dentro de un bloque cuántico todavía se rechaza. El mensaje es «classical branch inside
quantum control is not supported yet». La fuente y el diagnóstico se conservan como limitación
reproducida; no se cuentan como un estado cuántico aprobado.

Una continuación incorporó las pruebas a un
[validador C recurrente](../pointer-differences-maintained/maintained-summary.json) en otro
candidato de 137 entradas: pasaron **150 comprobaciones normales y 150 con ASan/UBSan**. Cada
configuración comprueba 80 ejecuciones nativas, 50 rechazos y 20 circuitos de acción e inversa.
Dentro de esos circuitos se contrastan 40 estados; las 40 referencias en C ordinario se cuentan
aparte. Diez de los 50 rechazos conservan explícitamente el caso válido aún no implementado de la
condición clásica dentro del control cuántico.

El validador se conectó a los scripts de construcción, ejecución normal, instrumentación y archivo
del candidato. Se reutilizaron las herramientas del experimento anterior y se recompiló el nuevo
validador en ambas configuraciones. Todavía faltan la regresión amplia de esta extensión y su
aplicación al compilador principal, además de los contratos generales de punteros, tamaños, layouts
y llamadas entre módulos.

La primera matriz se detuvo porque esperaba un diagnóstico demasiado específico al restar un puntero
nulo. Ese caso no tiene una resta definida por C11 y el compilador lo rechazó mediante su control de
escapes existente. Se conservó el fallo de expectativa y solo se repitieron las diez combinaciones
afectadas, sin borrar los resultados anteriores.

El [resumen verificable](experimental-summary.json) identifica las 136 entradas de fuentes y los
hashes de los once artefactos originales. Los archivos comprimidos conservan las fuentes, los
programas de prueba, los circuitos exportados, los controladores y los resultados. Este documento
explicativo se añadió después de cerrar esa cápsula y no forma parte de sus once artefactos.

La ciberseguridad permanece clásica y sin cambios. Las comprobaciones usan simulación; no prueban
ejecución en hardware cuántico ni aceleración frente a CPU.
