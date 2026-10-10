# Especificación objetivo y aceptación de la versión 1.0

Este documento fija el alcance exigido para 1.0. Es un contrato de publicación, no una certificación de que la implementación actual lo satisface. El [README](../README.md) registra capacidades y evidencia. Un dominio pequeño probado exhaustivamente no cierra un requisito general.

## Subconjunto cerrado

1.0 comprende enteros de anchura fija de 1 a 64 bits, booleanos, arrays fijos y productos enteros, préstamos con dirección clásica, resultados escalares propios, funciones unitarias componibles y control CPU explícito. Se conservan promociones, conversiones, signo y orden de los efectos del C definido del destino GCC declarado.

Calcular un resultado nuevo conserva sus entradas. Mover transfiere propiedad. Medir/resetear son operaciones explícitas. Limpiar exige demostrar la inversa, sus controles y las versiones de sus dependencias. Los rechazos protegen el contrato del subconjunto; no cuentan como implementación de una construcción pendiente.

Quedan fuera: QPU, memoria cuántica dinámica, QRAM, direcciones en superposición, coma flotante cuántica, todo C11, recursión cuántica general y bucles coherentes generales. Los saltos CPU sin prueba de vida se rechazan. Un fork de GCC no es requisito; justificar el punto de transformación sí lo es.

Anchura del lenguaje, trabajo del circuito y simulación son límites distintos. La referencia actual exporta hasta 128 qubits físicos y simula hasta 24. Agotar un presupuesto da un diagnóstico; no autoriza enumerar silenciosamente un dominio grande.

## Orden y criterios verificables

| Prioridad | Contrato | Evidencia para cerrarlo |
|---|---|---|
| P1. Aritmética | Multiplicación, división, resto y comparaciones entre registros sin tablas. | Amplitudes e inversas contra C, más de 12 bits relevantes, cotas y CSV de puertas, auxiliares y profundidad. |
| P2. Búsqueda | Separar oráculo y estrategia; cantidades conocidas/desconocidas, candidato verificado y fallo acotado. | Cero, una, varias y todas las soluciones sin un censo del dominio. |
| P3. Flujo | Propiedad, préstamos y dependencias correctos en todos los caminos aceptados. | Uniones, invariantes de bucles, medidas, retornos y temporales; casos positivos y negativos. |
| P4. Composición | Compilación separada, ABI tipada, resultados transferidos, control e inversa. | Módulos enlazados, alias rechazados, limpieza y fases relativas bajo control. |
| P5. GCC | Semántica conservada a través de la frontera del plugin. | Diferenciales entre optimizaciones, promociones, casts, cortocircuitos y efectos evaluados una vez. |
| P6. Validación | Importar el QASM original en otra implementación y tener CI/fuzzing reproducibles. | Estados completos, mediciones, registros de ejecución y reproducciones de fallos. |
| P7. Ejecución | Separar planes estáticos, parámetros CPU y control CPU. | Contadores que demuestren ausencia de parses y síntesis repetidos; estado final equivalente. |

P1 y P2 son los primeros bloqueos de escalabilidad; P3 sigue inmediatamente. P5 y P6 acompañan cada cambio.

## P1: síntesis y complejidad

El cálculo nuevo implementa `|a,b,0> -> |a,b,f(a,b)>`, con trabajo devuelto a cero. La copia al resultado es XOR, de modo que el mismo cálculo permite descomputarlo. La actualización del destino requiere una transformación inyectiva independiente.

Se exige suma/resta, multiplicación modular sin signo, cociente/resto con divisor probado distinto de cero, y las seis comparaciones entre registros. Las promociones con signo deben conservarse y probar ausencia de overflow antes de reducir la salida. División por cero, overflow con signo y shifts indefinidos se rechazan. La evaluación condicional conserva el cortocircuito.

La matriz obligatoria compila anchuras 4, 8, 12 y 16 con tablas desactivadas: dos entradas de 16 bits tienen 32 bits relevantes. Dominios pequeños completos se contrastan con C independiente, amplitudes, controles e inversas. Se publica CSV con puertas por clase, auxiliares máximos, qubits físicos y profundidad. MCX se cuenta como puerta lógica; su descomposición física tiene otro coste.

Cada algoritmo debe documentar su cota. Las construcciones por incrementos controlados actuales tienen hasta coste cúbico en puertas lógicas y espacio lineal para un operador binario. La composición necesita una cota por tamaño del árbol y una estrategia de temporales. Tokens, profundidad, términos simbólicos, trabajo real y memoria deben tener presupuestos globales: evitar tablas no garantiza escalabilidad.

La implementación incorpora `native/arithmetic.inc`, operandos negativos, división truncada hacia cero, resto con signo del dividendo, comparaciones generales y `--no-tables` en el backend. Demuestra divisores distintos de cero mediante rangos o bits constantes y excluye MIN/-1. Conserva una ruta finita optativa para expresiones pequeñas, casos no cubiertos y permutaciones. Las pruebas de rango más precisas, las composiciones grandes y el divisor de 64 bits siguen abiertos; P1 queda parcial.

## P2: búsqueda

Construir el oráculo no consulta cuántas entradas satisfacen el predicado. Evaluar un candidato medido está permitido; evaluar previamente todas las entradas para elegir la estrategia no lo está.

| Operación | Información | Resultado |
|---|---|---|
| `find_known(x,p,M)` | Promesa del llamador: `1 <= M <= N`. | Transfiere el registro amplificado; `P(r)=sin²((2r+1)asin(sqrt(M/N)))`. No comprueba la promesa contando. |
| `search(x,p,attempts)` | Cantidad desconocida, incluso cero. | Booleano CPU: `true` exige candidato medido que satisface `p`; `false` significa presupuesto agotado. |

`find(x,p)` adopta el contrato de `find_known(x,p,1)`. Es un cambio respecto del prototipo que contaba soluciones. Con varias soluciones se proporciona M o se utiliza `search`.

`search` mezcla, con igual probabilidad, muestras directas y un número uniforme de iteraciones entre cero y `ceil(sqrt(N))-1`. Bajo aleatoriedad uniforme independiente, un dominio no vacío tiene éxito por intento de al menos `1/8`: el fallo tras t intentos es a lo sumo `(7/8)^t`. Para M mayor o igual que N/2 bastan las muestras directas; para los otros M el promedio de Grover es al menos `1/4`, pues `K*sin(2*theta) >= 2*sqrt(1-1/N)`, con `K=ceil(sqrt(N))`.

El runtime usa un PRNG reproducible y muestreo sin sesgo modular. La cota corresponde al modelo probabilístico, no garantiza el resultado de una semilla particular. Cada éxito se verifica: no hay falsos positivos. Con cero soluciones se agotan los intentos sin demostrar inexistencia. La operación incluye medición/reset y repreparación entre intentos, y deja el registro disponible con el último candidato medido.

La aceptación exige cero/una/varias/todas las soluciones, dominio mayor de 12 bits, limpieza, presupuesto finito y rechazo de exportar feedback CPU como QASM plano. Las regresiones cubren esos contratos; faltan campañas estadísticas amplias y estrategias adaptativas con mejor coste de consultas.

## P3: flujo y propiedad

Cada punto debe disponer del estado de identidad, propietario, préstamo, disponibilidad, medida/reset, rango e inversa por registro. Una unión conserva solo hechos ciertos en todos sus predecesores. Un bucle incluye cero iteraciones cuando corresponda y prueba su arista de retorno.

Se exige `if` cuántico en bucles CPU con propietarios estables y mediciones que alimenten decisiones CPU. Los auxiliares se limpian cada iteración. Consumo en una sola rama, uso tras movimiento, préstamo mutable solapado y cambio de una dependencia pendiente se rechazan.

La implementación une conservadoramente ramas y bucles e invalida rangos antes del control CPU dinámico. Admite esas combinaciones, resultados escalares propios con retorno terminal y retornos puros condicionales. Creación dinámica de propietarios, retornos tempranos con vida pendiente, `goto`, `switch` y transferencias con distinta identidad por rama siguen pendientes. La IR no reconstruye todavía un CFG completo: P3 queda parcial.

## P4: funciones y ABI

La ABI debe declarar representación, anchura, signo, modo de préstamo/consumo, resultados, efectos y obligaciones de limpieza. Un resultado propio puede escapar por transferencia; una referencia prestada no puede salir como propietario nuevo.

Las funciones unitarias admiten control e inversa. La inversa invierte orden y ángulos; el control conserva también la fase global, que se vuelve relativa. Se rechazan medición, reset, búsqueda, feedback CPU y trabajo sucio dentro de esos módulos.

Hay una ABI nativa QIR con `qborrow`, `qc_circuit_compile` y `qc_circuit_apply`, y composición desde C mediante `circuit`/`inverse_circuit`. El archivo independiente se incorpora al compilar; el runtime conserva su plan. [circuit-abi.md](circuit-abi.md) fija formato y restricciones. Esto no equivale aún a compilar/enlazar funciones C `quant` arbitrarias entre unidades ni a retornar productos propios mediante la ABI: P4 queda parcial.

## P5: GCC

El plugin transforma en `PLUGIN_PRE_GENERICIZE`; las transformaciones anteriores forman parte de la frontera de confianza. `volatile` preserva determinados accesos y no demuestra que sobrevivan todas las promociones y condiciones de comportamiento definido.

Se exigen referencias CPU y diferenciales a `-O0`, `-O1`, `-O2`, `-O3` y `-Os`: promociones, narrowing, signo, cortocircuitos y efectos. Las secuencias puras introducidas por GCC se conservan como `sequence` para validar también la expresión descartada. Un estrechamiento temprano ambiguo de multiplicación se rechaza si el dominio podría desbordar el `int` original; se requiere un resultado sin signo de anchura completa antes de estrechar.

Hay generación reproducible de programas, semilla y archivo de reproducción. Faltan más patrones y versiones de GCC para cerrar P5; no se afirma una prueba general de todas las transformaciones anteriores al plugin.

## P6: validación y CI

La referencia externa importa el QASM original y compara estados completos hasta una fase global común, con error máximo de amplitud `1e-10`. Casos obligatorios: superposición, entrelazamiento, fases relativas, controles abiertos/cerrados, Fourier/inversa, Grover completo y mediciones correlacionadas.

Se declaran tolerancia, muestras y semillas de medición. La campaña actual usa 8192 muestras por circuito y Hoeffding con unión de eventos, con probabilidad de falso rechazo familiar a lo sumo `1e-6` bajo el modelo de muestras independientes. Ocho circuitos comparan amplitudes y tres comparan transcripciones completas entre el runtime nativo y Aer, incluyendo reset de una entrada entrelazada y mediciones intermedias. La cota estadística no certifica una semilla particular del PRNG. No valida feedback dinámico exportado, ruido ni todos los canales de reset.

El workflow construye con GCC 11 y ejecuta suite nativa, ASan/UBSan de toda la cadena propia, crecimiento sin tablas, frontend generado y Qiskit/Aer. Se fijan dependencias y se archivan registros/reproducciones. Preparar el workflow no registra una ejecución remota. Faltan campañas mayores y más medidas intermedias: P6 queda parcial.

## P7: reutilización

Plan estático, parámetros CPU y decisión CPU son capas distintas. Se verifican vida y tipos cada llamada; parser y síntesis no deben repetirse mientras sean válidas las precondiciones del plan.

Se conservan documentos parseados, planes de expresiones/sumas y módulos unitarios compilados. Las claves incluyen expresión, cables, rangos y controles. El trabajo privado se reutiliza solo si sus cables no pertenecen a un registro vivo. Parámetros de suma seleccionan variantes por valor; índices CPU seleccionan identidades sin medirlas. Las cachés tienen límites explícitos.

Ocho cálculos y sus inversas prueban cinco parses, una síntesis y quince aciertos del plan, además del estado. Falta enlazar de forma general puertas ya sintetizadas y plantillas parametrizadas para evitar también la síntesis de la primera llamada. P7 queda parcial.

## Publicación

No se anuncia 1.0 hasta cerrar los siete requisitos con implementaciones, diagnósticos y evidencia reproducible para el subconjunto definido. QPU, memoria dinámica y ampliar C no sustituyen ninguna condición de corrección o escalabilidad.
