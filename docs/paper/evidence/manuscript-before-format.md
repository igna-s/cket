# C Ket (|c> / cket)

## Una extensión experimental de C11 para datos cuánticos, síntesis reversible y ejecución híbrida

**Ignacio Schwindt**

Manuscrito técnico del proyecto · Versión estudiada: 0.4.0 experimental · 11 de octubre de 2026

**Resumen.** Este trabajo presenta C Ket, denominado `|c>` y distribuido como `cket`: una extensión experimental de C11 que expresa registros cuánticos mediante declaraciones `quant`, operaciones enteras y condiciones coherentes. El sistema utiliza el frontend real de GCC, transforma el programa mediante un plugin y representa sus efectos cuánticos en una IR JSON tipada. Un motor implementado en C valida dependencias, sintetiza circuitos reversibles y ejecuta un simulador de vector de estado enlazado al programa nativo. La propuesta combina propiedad de registros, préstamos comprobados, medición explícita y descomputación de temporales; incorpora Fourier, búsqueda por amplificación de amplitud y módulos unitarios controlables e invertibles. La evaluación reproducida para este manuscrito comprende 354 pruebas nativas, 426 comprobaciones sobre 16 ejemplos, 145 compilaciones diferenciales, 24 circuitos aritméticos sin tablas y 11 circuitos exportados contrastados con QuEST. Los resultados documentan corrección en los casos evaluados y costos de circuitos de hasta 32 bits relevantes; no establecen una prueba formal del compilador ni una ventaja temporal sobre hardware clásico. Se describen por separado el límite de representación de circuitos de 128 qubits y el límite de simulación de 24 qubits, junto con las restricciones actuales de control de flujo, ABI y síntesis.

**Palabras clave:** lenguajes cuánticos; C11; GCC; computación reversible; descomputación; OpenQASM 3; simulación de amplitudes.

## 1. Introducción

Programar un algoritmo cuántico exige conservar información mientras se manipulan amplitudes y fases. Las expresiones habituales de un lenguaje imperativo no pueden convertirse indiscriminadamente en sobrescrituras de registros: una transformación coherente debe ser reversible, y observar un resultado requiere una operación física distinta del cálculo unitario. Este problema aparece incluso al traducir una comparación o descartar un valor temporal.

C Ket explora una interfaz cercana a C: el programador declara datos cuánticos y escribe operaciones sobre ellos; el compilador decide cómo expresar esas operaciones mediante circuitos y cuándo puede recuperar el espacio auxiliar. El código clásico conserva la ejecución de CPU y las reglas de tipos de GCC. La frontera entre ambos dominios se hace visible mediante `measure` y `reset` [1].

Las contribuciones implementadas son una integración con GCC basada en atributos y transformaciones del AST; una IR cuántica tipada con validación de operaciones y dependencias; síntesis reversible con limpieza de espacio auxiliar; y una ejecución híbrida nativa acompañada por exportación estática y herramientas de validación. Su interés reside en la combinación concreta de estas decisiones dentro de un prototipo funcional. No se reclama prioridad histórica para la programación cuántica de alto nivel ni para la descomputación automática.

<!-- page -->

## 2. Modelo semántico y frontera clásica

Un registro de ancho w representa estados en un espacio de dimensión 2 elevado a w. Para varios registros, el estado global puede contener correlaciones que impiden describir cada variable como un valor independiente. El modelo de referencia es:

$$
|\psi\rangle = \sum_{x=0}^{2^n-1}\alpha_x|x\rangle,\qquad \sum_x|\alpha_x|^2=1.
$$

La palabra `quant` es una macro de `include/quantum.h` que introduce el atributo `cket_quant`, registrado por el plugin. Por ello, la propuesta no añade un parser independiente ni modifica el lexer de GCC. El atributo identifica declaraciones y funciones para el análisis cuántico; el prototipo no implementa todavía una propagación nativa completa de este calificador en el sistema de tipos de GCC [1, 2].

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t value = 7;
    value += 5;
    printf("%u\n", (unsigned)measure(value));
    return 0;
}
```

Este programa prepara un estado de base, realiza suma modular reversible y obtiene 12 al medir. La suma sobre un registro unsigned de ancho w implementa una permutación de los estados de base:

$$
U_a|x\rangle = |(x+a)\bmod 2^w\rangle.
$$

GCC conserva promociones, conversiones y signedness. En particular, `x*x` con `x` de tipo `uint8_t` puede promoverse a `int`; no debe interpretarse como multiplicación truncada a ocho bits antes de comparar. La aritmética signed aceptada exige las pruebas de rango disponibles, y la división exige demostrar un divisor distinto de cero. No se prueba la ausencia de comportamiento indefinido en todo programa C ni en todas las transformaciones previas de GCC.

| Operación | Significado contractual |
|---|---|
| `quant T x = c` | Preparar almacenamiento limpio con un valor de base. |
| `superposition(0, 15)` | Ejemplo de dominio uniforme con límites constantes aceptados. |
| `quant T y = f(x)` | Calcular un resultado preservando sus entradas; puede entrelazarlas. |
| `measure(x)` | Colapsar explícitamente y devolver el tipo entero C subyacente. |
| `reset(x)` | Destruir explícitamente información y dejar el registro en cero. |

Los intervalos uniformes arbitrarios o calculados por CPU permanecen fuera del contrato actual. Una inicialización de base sí puede recibir un valor clásico evaluado una vez.

<!-- page -->

## 3. Arquitectura de compilación y ejecución

El sistema separa análisis del programa, representación cuántica, validación, síntesis y ejecución. La Figura 1 resume las rutas implementadas. La ruta nativa conserva el control clásico de C; la ruta de exportación opera sobre la parte cuántica que admite representación estática [1].

![Figura 1. Arquitectura de C Ket. El programa nativo mantiene el control de CPU y llama al runtime; los documentos estáticos alimentan validación, recursos y exportación. Diagrama elaborado a partir de src/cket.c, src/cket_plugin.cpp, src/cket_runtime.c y src/engine/.](figures/fig01_architecture.png)

El driver `src/cket.c` selecciona GCC y comprueba su compatibilidad con la versión utilizada para construir el plugin. Este último se implementa en C++17 porque usa la API interna de GCC, registra atributos y actúa en `PLUGIN_PRE_GENERICIZE`, un punto de extensión documentado por GCC [2]. Las reglas de expresión, propiedad y transformación se distribuyen también en `quantum_semantics.inc` y `quantum_frontend.inc`.

La IR propia se identifica como `cket.qir`, versión 1. Contiene anchos, signedness, expresiones y operaciones como `qalloc`, `qcompute`, `quncompute`, `qmeasure` y `qphase`, además de marcadores de control clásico. El nombre QIR se usa en el repositorio como abreviatura de representación intermedia cuántica; este formato JSON es distinto de LLVM QIR. No se implementa emisión LLVM.

El ejecutable nativo enlaza `src/cket_runtime.c` con `libcket_quantum.a`. Las llamadas del programa transmiten instrucciones y argumentos clásicos al motor. La ejecución del binario no vuelve a invocar GCC ni vuelve a analizar el archivo C; el motor sí puede analizar las instrucciones JSON y sintetizar planes durante la primera llamada. Sus cachés reutilizan documentos y planes, manteniendo comprobaciones de disponibilidad y versiones en cada invocación.

El backend `src/cket_backend.c` es una interfaz de línea de comandos del mismo motor. Proporciona validación, exportación, métricas y consulta de amplitudes para depuración. El driver, runtime, parser JSON, verificador, sintetizador, simulador y programas de prueba están implementados en C. Python se utiliza únicamente para producir las figuras y maquetar este manuscrito; no es una dependencia del compilador ni del programa cuántico.

<!-- page -->

## 4. Propiedad, préstamos y descomputación

Para una función clásica definida f, un cálculo reversible preserva la entrada y acumula el resultado en un destino auxiliar. La construcción conceptual utilizada es:

$$
U_f|x\rangle|y\rangle = |x\rangle|y\oplus f(x)\rangle.
$$

Aplicar nuevamente esta transformación elimina f(x) del destino si las entradas permanecen disponibles. La reversibilidad y la recuperación de temporales tienen antecedentes en Bennett [3]; el prototipo añade comprobaciones específicas sobre registros, controles y dependencias de la IR [1].

![Figura 2. Cálculo, uso coherente y descomputación de un predicado. La fase queda en la entrada y el auxiliar vuelve a cero. Las dependencias y los controles deben conservar el certificado requerido para aplicar la inversa.](figures/fig02_uncomputation.png)

Cada `qcompute` registra su expresión creadora, el patrón normalizado de controles y las identidades y versiones de sus dependencias. `quncompute` exige que esos datos sigan siendo compatibles. Mutar, medir, resetear o liberar una dependencia necesaria para una inversa pendiente produce un diagnóstico. Una fase diagonal puede conservar el certificado porque modifica amplitudes sin alterar los valores de base de los que depende el cálculo; puede, sin embargo, invalidar la prueba de preparación uniforme necesaria para búsqueda.

La comprobación actual de la expresión usa igualdad del texto JSON, además de controles y versiones. Es una condición conservadora: dos serializaciones matemáticamente equivalentes pueden ser rechazadas. Esta regla no constituye un procedimiento general de equivalencia de programas. Tras la inversa, el motor acredita el destino como cero y elimina el certificado. En simulación, la probabilidad residual fuera de cero debe ser menor que `1e-10` antes de reutilizar almacenamiento; la exportación no depende de amplitudes simuladas.

Una declaración `quant T y = x` con tipos compatibles transfiere propiedad y consume el nombre anterior. Una expresión nueva como `quant uint8_t p = x % 2` preserva x y construye un resultado correlacionado. Por ello, estas dos formas de inicialización tienen contratos distintos. Los préstamos permiten operar sobre registros existentes sin transferirlos, con controles de aliasing y de mutabilidad.

Los temporales elegibles se descomputan después de su último uso, en orden inverso de dependencia. Todo propietario debe terminar su vida mediante medición explícita, reset, transferencia o descomputación segura. El compilador rechaza abandonos que no puede justificar y no introduce mediciones ocultas para resolverlos.

<!-- page -->

## 5. Control coherente, correlaciones y Fourier

Una condición cuántica no selecciona una rama observando un valor: controla unitariamente sus operaciones. En cambio, una condición clásica posterior a `measure` es una decisión de CPU. El ejemplo de entrelazamiento del repositorio ilustra la primera interpretación [1].

```c
quant bool a = 0;
quant bool b = 0;
fourier(a);
if (a) b ^= 1;
unsigned first = measure(a);
unsigned second = measure(b);
```

Sobre un bit, Fourier actúa como Hadamard. El XOR controlado posterior produce el estado de Bell mostrado en la Figura 3. La distribución conjunta asigna probabilidad 1/2 a 00 y 11, y probabilidad cero a 01 y 10. Medir parcialmente conserva las correlaciones del estado restante.

![Figura 3. Circuito de Bell y distribución conjunta ideal antes de medición. Las barras se derivan de la evolución analítica del circuito del ejemplo; no representan frecuencias experimentales ni una ejecución en QPU.](figures/fig03_bell.png)

Bajo control cuántico se aceptan operaciones coherentes compatibles y fases; se rechazan I/O, escrituras de CPU, medición, reset y saltos clásicos incompatibles. Si la condición requiere un predicado auxiliar, también debe ser posible invertirlo sin que sus entradas hayan cambiado. Esta restricción evita convertir un `if` familiar en una transformación que pierda información.

Para un registro completo de ancho w, la convención de Fourier implementada es de exponente positivo y orden little-endian:

$$
F_w|x\rangle=\frac{1}{\sqrt{2^w}}\sum_{k=0}^{2^w-1} e^{2\pi i xk/2^w}|k\rangle.
$$

`inverse_fourier` cambia el signo del exponente. La síntesis utiliza un número cuadrático de operaciones respecto del ancho y admite control coherente sin tablas de dominio. La transformación invalida pruebas de valor y uniformidad; se rechaza cuando rompe una inversa pendiente. Los tests comparan bases pequeñas con una DFT matemática independiente y tolerancia `1e-10`.

Los módulos `circuit` e `inverse_circuit` remapean registros de una IR unitaria, preservan fases relativas bajo controles y exigen devolver limpios sus auxiliares privados. Sus parámetros son préstamos tipados por representación; todavía no equivalen a una ABI completa para funciones C cuánticas entre unidades de traducción.

<!-- page -->

## 6. IR verificada y síntesis de circuitos

El verificador revisa campos permitidos, duplicados, tipos, aridades, constantes, conversiones explícitas y controles. Los anchos aceptados van de 1 a 64 bits. Los controles deben ser registros Booleanos disponibles, sin contradicciones ni alias con el destino. Los ángulos de fase deben ser finitos. Todos los brazos de una expresión reciben validación estructural; su evaluación aritmética preserva el cortocircuito de C [1].

La siguiente instrucción aislada ilustra la representación de un cálculo modular. Requiere que los registros referidos existan y que el destino esté limpio; no es por sí sola un documento ejecutable completo.

```json
{
  "op": "qcompute", "id": 1,
  "expr": {
    "op": "add", "bits": 8, "signed": false,
    "args": [
      {"op": "reg", "id": 0, "bits": 8, "signed": false},
      {"op": "const", "bits": 8, "signed": false, "value": 5}
    ]
  }
}
```

El motor mantiene pruebas independientes de valor conocido, bits iguales a cero y preparación uniforme intacta. Metadatos como `known` o `active` no bastan para establecer estas propiedades: deben coincidir con lo demostrado por el motor. Preparación y reset establecen hechos; transformaciones y mediciones los actualizan o invalidan de forma conservadora.

La síntesis combina tres rutas. La ruta simbólica compone funciones de bits y ciertos predicados sin enumerar el dominio. Los circuitos de registros implementan aritmética y comparaciones con espacio auxiliar reversible, acumulación del resultado e inversión del trabajo. La ruta finita opcional admite hasta 12 bits relevantes mediante formas normales algebraicas o minterms; las permutaciones generales conservan ese límite. `--no-tables` desactiva esta última alternativa. Las pruebas de crecimiento de la Sección 10 utilizan explícitamente ese modo.

| Componente | Límite o condición actual |
|---|---|
| Síntesis simbólica | 256 términos por bit; presupuesto de 2.000.000 de pasos. |
| Expresiones IR | 100.000 tokens; profundidad de expresión de 128 niveles. |
| Entrada documental | Hasta 64 MiB. |
| División y resto | Divisor no nulo probado; restricciones de rango y workspace. |
| Módulos anidados | Hasta 16 niveles; cuerpo unitario y auxiliares limpios. |
| Expansión de bucles fijos | Hasta 4096 iteraciones con comprobaciones. |

Los presupuestos limitan uso de recursos y producen diagnósticos; no prueban síntesis eficiente para toda expresión. Los marcadores de CPU se comprueban estructuralmente, pero la IR no reconstruye un grafo completo de todos los caminos clásicos. Tampoco es una demostración formal de corrección de todas las transformaciones de GCC.

<!-- page -->

## 7. Búsqueda: contratos y amplificación de amplitud

Grover ofrece una reducción cuadrática del número de consultas a un oráculo en búsqueda no estructurada [4]. C Ket construye el oráculo desde un predicado puro y utiliza esa evolución para `find` y `find_known`; el costo de sintetizar el predicado sigue formando parte del costo del programa [1].

```c
quant uint8_t x = superposition(0, 15);
quant uint8_t answer = find(x, x * x == 9);
printf("%u\n", (unsigned)measure(answer));
```

`find` exige una única solución; `find_known(x, p, M)` recibe un conteo conocido entre 1 y N. El conteo es una promesa del llamador y no se verifica enumerando soluciones. Ambas operaciones requieren un dominio uniforme intacto y transfieren su propiedad al resultado.

El análisis de amplificación da la probabilidad ideal después de r iteraciones, para M elementos marcados de un dominio N [5]:

$$
\theta=\arcsin\sqrt{M/N},\qquad P_r=\sin^2((2r+1)\theta).
$$

![Figura 4. Amplificación ideal para N=16 y M=1. El backend compara los enteros alrededor del máximo continuo y selecciona r=3; la probabilidad teórica es 0,9613189697. La curva se calcula con la fórmula y se contrasta con el test de amplitudes del proyecto.](figures/fig04_grover.png)

El backend estima `pi/(4*theta)-1/2`, compara floor y ceil, y escoge el que da mayor probabilidad. Para el ejemplo, la solución es 3 y la probabilidad ideal ronda 96,13 %. El resultado sigue siendo probabilístico aun sin ruido; una semilla reproducible no garantiza éxito en todos los programas. El test nativo comprueba la amplitud correspondiente y el retorno a cero del auxiliar del oráculo.

Para un conteo desconocido, incluido cero, `search(x, p, attempts)` realiza una estrategia híbrida con medición y verificación clásica. Mezcla muestreo directo con cantidades aleatorias de iteraciones de Grover, reinicia entre intentos y acepta presupuestos de 1 a 4096. Devuelve true solamente tras verificar un candidato; false expresa agotamiento del presupuesto. La cota documentada de fallo `(7/8)^attempts` presupone que existe una solución y aleatoriedad uniforme independiente. Es un contrato probabilístico del proyecto, no una garantía por semilla ni una atribución literal del algoritmo a BBHT [1, 5]. La retroalimentación de CPU impide su exportación como circuito estático plano.

<!-- page -->

## 8. Runtime, simulador y costos de memoria

El runtime crea un contexto de simulación dentro del ejecutable y ejecuta los efectos cuánticos cuando el control clásico los alcanza. Acepta argumentos enteros, ángulos de fase y selección de índices de CPU mediante interfaces diferenciadas. `CKET_SEED` permite fijar la semilla; la memoria del contexto se libera al terminar [1].

El simulador almacena amplitudes `double complex` en little-endian. H combina pares de amplitudes; X y sus controles permutan entradas; una fase multiplica las seleccionadas por un exponencial complejo. Estas operaciones unitarias no muestrean. La medición utiliza probabilidades de Born, elimina amplitudes incompatibles y renormaliza. El reset solicitado por el programa se implementa mediante una trayectoria de colapso y corrección.

![Figura 5. Memoria teórica del vector de estado para amplitudes complejas de 16 bytes. Las cifras no son mediciones de RAM total: excluyen puertas, registros, cachés y overhead. La línea marca el límite de 24 qubits del simulador de C Ket.](figures/fig05_memory.png)

El espacio del vector crece como 16 multiplicado por 2 elevado a n bytes en la representación de referencia. A 16 qubits ocupa 1 MiB; a 24, 256 MiB; a 32, 64 GiB. Declarar solo un soporte inicial pequeño no elimina los bits del ancho del registro. Los auxiliares también cuentan. La reutilización de wires limpios evita nuevas asignaciones, pero la dimensión ya alcanzada del vector no se reduce.

| Dimensión del soporte implementado | Alcance |
|---|---|
| Ancho de un entero cuántico | Hasta 64 bits de representación. |
| Almacenamiento y exportación de puertas | Hasta 128 qubits físicos representados. |
| Simulación local de vector de estado | Hasta 24 qubits físicos. |

Estas cifras describen restricciones diferentes. Un programa con `quant int` de 32 bits puede exportarse aunque no sea ejecutable en este simulador. La división de 64 bits requiere workspace adicional todavía no soportado. Exportar 128 qubits no implica simularlos ni mapearlos a un dispositivo con 128 qubits disponibles.

La consulta `--state` expone amplitudes antes de las mediciones del documento como herramienta de depuración del simulador. No es una capacidad de observación de hardware cuántico y no modifica la semántica del ejecutable. La implementación actual no incorpora backend QPU, ruido físico, corrección de errores ni optimización para una topología concreta.

<!-- page -->

## 9. Metodología de evaluación y procedencia

La revisión se fija al commit `1ebb2e308e61bed5632798ded5beecf77b6b9a9c` y a GCC 11.4.0 sobre Ubuntu 22.04 en WSL2 x86_64. Se reconstruyó el proyecto y se ejecutaron sus campañas normales de pruebas, crecimiento, frontend y validación QASM. Los archivos en `docs/paper/evidence/` conservan comandos indirectos mediante el script reproducible, el entorno, logs y CSV. El informe de auditoría identifica qué se repitió y qué constituye evidencia histórica [1].

| Campaña reproducida | Resultado | Qué comprueba |
|---|---|---|
| Pruebas nativas | 354 aprobadas; 0 fallidas | Semántica, amplitudes, IR inválida, dependencias y CLI. |
| Ejemplos completos | 426 comprobaciones; 16 ejemplos | Contratos de ejecución, entradas y exportación en cinco niveles GCC. |
| Diferencial de frontend | 145 compilaciones | 5 casos fijos y 24 generados, cada uno en cinco niveles. |
| Crecimiento sin tablas | 24 circuitos | 6 operadores por 4 anchos; recursos y límites de crecimiento. |
| Referencia QASM independiente | 11 circuitos | 8 estados/probabilidades y 3 canales con medición/reset. |

Los niveles son `-O0`, `-O1`, `-O2`, `-O3` y `-Os`. El generador diferencial usa semilla 20261011 y los programas ejecutados usan semilla 7. Esas 145 compilaciones no representan 145 algoritmos independientes ni cobertura de todas las optimizaciones de GCC. Las 426 comprobaciones tampoco deben sumarse a las 354 como si fueran unidades homogéneas de cobertura.

La suite nativa contiene, además, campañas internas de 1024 expresiones tipadas y 4096 mutaciones de IR. Son partes de sus casos agregados, no miles de nuevos tests independientes para aumentar la cifra principal. También incluye pruebas exhaustivas sobre dominios pequeños y casos de correlación, fase, inversión y rechazo de operaciones inválidas.

El validador externo lee el OpenQASM emitido con un importer separado y lo ejecuta en el backend C de QuEST 3.7.0 [10]. Su lectura no consulta la IR original ni ejecuta puertas a través del simulador nativo. Para ocho circuitos compara amplitudes hasta fase global y probabilidades finales con tolerancia `1e-10`; la comparación de fase relativa sigue siendo significativa. Para tres canales contrasta distribuciones de transcripciones completas con 8192 ejecuciones por circuito y por implementación, usando una tolerancia estadística explícita. También rechaza ocho artefactos QASM malformados.

El importer cubre el subconjunto estático emitido por `cket`, no toda la especificación OpenQASM 3. La nota de lanzamiento registra 353 tests y una campaña histórica con Qiskit/Aer. Aquí se usan los 354 tests del estado auditado y la campaña C/QuEST actual. Los logs de ASan/UBSan ya existentes son evidencia anterior; esta auditoría no los presenta como una nueva ejecución instrumental.

<!-- page -->

## 10. Resultados de síntesis: puertas lógicas

La prueba `tests/benchmark_synthesis.c` construye dos entradas unsigned de ancho w, preparadas uniformemente, y un destino. Examina seis operadores para w igual a 4, 8, 12 y 16, desactivando tablas. Para división y resto, el segundo operando se combina con uno mediante OR para garantizar un divisor no nulo. Las salidas aritméticas tienen ancho w y la comparación produce un bit [1].

![Figura 6. Puertas lógicas en 24 circuitos sin tablas; eje vertical logarítmico. Suma y resta coinciden. Fuente: evidence/synthesis-growth.csv reproducido; incluye preparación y cálculo, sin limpieza posterior del resultado final.](figures/fig06_gate_growth.png)

| Operador | w=4 | w=8 | w=12 | w=16 |
|---|---|---|---|---|
| Suma | 40 | 112 | 216 | 352 |
| Resta | 40 | 112 | 216 | 352 |
| Multiplicación | 52 | 264 | 764 | 1680 |
| División | 316 | 1432 | 3732 | 7600 |
| Resto | 324 | 1448 | 3756 | 7632 |
| Menor que | 49 | 97 | 145 | 193 |

El conteo suma H, X, CNOT, Toffoli, MCX, PHASE y GLOBAL_PHASE del reporte. Las preparaciones uniformes están incluidas. Los documentos del benchmark conservan el resultado final: el costo de esta carga no debe presentarse como el de un programa completo que descomputa también ese resultado. La limpieza de los auxiliares privados utilizados en aritmética sí forma parte de la síntesis.

A ancho 16, la comparación usa 193 puertas frente a 7632 para resto; división y resto requieren muchas más operaciones que las expresiones aditivas. Las observaciones describen estos circuitos y no permiten inferir una complejidad asintótica general para cualquier expresión C. El test de crecimiento limita razones sucesivas y workspace como comprobación de regresión; una aprobación no demuestra optimalidad.

Cada MCX cuenta como una puerta lógica antes de su descomposición física. Los costos en un dispositivo dependen de sus puertas nativas, conectividad y aproximación de fases. Los controles abiertos añaden X en QASM y pueden cambiar el total exportado. No se midieron tiempos de pared ni aceleración frente a código clásico.

<!-- page -->

## 11. Resultados de workspace y profundidad

Las diferencias de memoria auxiliar explican parte del costo de la síntesis y muestran por qué exportar no equivale a simular. La Figura 7 presenta el pico de qubits del mismo experimento. La línea de 24 qubits sirve como referencia del simulador nativo, no como límite de construcción de circuitos [1].

![Figura 7. Pico de qubits representados por circuito y ancho. Suma, resta y multiplicación coinciden. Fuente: CSV reproducido; la línea de 24 qubits marca el límite del simulador local.](figures/fig07_workspace.png)

| Operador a w=16 | Puertas | Auxiliares | Pico de qubits | Profundidad lógica |
|---|---|---|---|---|
| Suma | 352 | 16 | 64 | 246 |
| Resta | 352 | 16 | 64 | 276 |
| Multiplicación | 1680 | 16 | 64 | 1634 |
| División | 7600 | 50 | 98 | 5538 |
| Resto | 7632 | 66 | 114 | 5540 |
| Menor que | 193 | 17 | 50 | 41 |

Para suma, resta y multiplicación, dos entradas y un resultado aportan 48 qubits lógicos a ancho 16, mientras el pico total llega a 64. Resto alcanza 114 qubits, todavía dentro del límite de representación de 128, pero muy por encima de los 24 permitidos para amplitudes. Estos puntos se obtuvieron por síntesis y contabilización de recursos; no por simulación de vectores de 50 a 114 qubits.

La profundidad cuenta dependencias de wires en la representación de puertas del motor. No equivale a duración física, profundidad transpilada para una arquitectura, T-depth ni costo de corrección de errores. Aunque suma y resta tengan el mismo total de puertas, sus profundidades difieren en esta síntesis, lo que indica que el conteo agregado no describe por sí solo el paralelismo.

Las cuatro anchuras estudiadas tienen 8, 16, 24 y 32 bits relevantes de entrada. Esta evidencia demuestra que esas familias aceptadas se construyen sin el fallback finito de 12 bits. No demuestra soporte general de registros de 64 bits para todas las operaciones ni crecimiento eficiente de composiciones arbitrarias. La separación entre ancho del tipo, soporte relevante, espacio auxiliar y capacidad de simulación debe mantenerse al interpretar los resultados.

El CSV completo acompaña al manuscrito y permite regenerar sus figuras y tablas.

<!-- page -->

## 12. Trabajo relacionado y límites de la propuesta

La recuperación reversible de temporales se conecta con el modelo de Bennett [3]. Silq propone un lenguaje de alto nivel con descomputación segura y anotaciones de tipos que rechazan programas no físicos [6]. Quipper aporta un enfoque funcional de generación y composición de circuitos a gran escala [7]. Estos antecedentes sitúan C Ket en una línea ya existente; no justifican atribuirle la invención del lenguaje cuántico de alto nivel.

| Trabajo o sistema | Relación con este proyecto |
|---|---|
| Bennett [3] | Base conceptual para preservar entradas y revertir trabajo auxiliar. |
| Silq [6] | Referencia para temporales y semántica de descomputación segura. |
| Quipper [7] | Referencia de composición y generación programática de circuitos. |
| OpenQASM 3 [8, 9] | Lenguaje de intercambio usado por la ruta de exportación. |
| QuEST [10] | Simulador externo empleado en la validación diferencial. |

La especificidad de C Ket consiste en integrar declaraciones y expresiones C con el AST de GCC, propiedad de registros e inversas verificadas, un runtime C enlazado y un backend reutilizado para simulación y exportación. Esta comparación es arquitectónica; no se realizaron benchmarks equivalentes de rendimiento, líneas de código o productividad contra Silq o Quipper.

OpenQASM 3 define controles y fases, además de capacidades de interacción clásica más amplias que las emitidas por este prototipo [8, 9]. La prohibición de exportar control de CPU arbitrario describe la ruta estática de `cket`; no una incapacidad general del estándar. Conservar una fase global de un módulo resulta relevante cuando ese módulo se controla y la fase pasa a ser relativa.

Persisten restricciones sustanciales. El análisis de flujo clásico realiza uniones conservadoras, pero no cubre un CFG completo con todos los efectos y vidas. Las funciones cuánticas se componen principalmente dentro de una unidad de traducción; los módulos QIR admiten una ABI por representación de registros, mientras los retornos de productos y el enlace general de funciones C `quant` siguen pendientes. Recursión, llamadas indirectas cuánticas y bucles coherentes generales no están implementados.

El subconjunto cuántico tampoco incluye flotantes generales, VLAs, unions, bitfields, asignación dinámica cuántica ni índices cuánticos generales. Algunos productos y préstamos sí existen, y la selección de un elemento mediante índice de CPU tiene un contrato limitado con comprobación de bounds. El compilador utiliza un marcador `volatile` interno provisional para preservar ciertos accesos frente a simplificaciones de GCC; las pruebas diferenciales acompañan esa estrategia, pero no la convierten en una garantía formal.

Las amenazas a la validez incluyen un único entorno de GCC de referencia, dominios y programas finitos, restricciones compartidas entre componentes del mismo motor y precisión de punto flotante sin presupuesto global de error. QuEST reduce la dependencia de una única implementación en los circuitos comprobados, pero no elimina esas amenazas. Los resultados se refieren al commit fijado y no certifican las metas abiertas de la versión 1.0.

<!-- page -->

## 13. Conclusiones y reproducibilidad

C Ket demuestra una ruta implementada para expresar datos cuánticos desde C11, conservar control clásico nativo y producir circuitos reversibles con comprobaciones de dependencias y limpieza. La evaluación reproducida cubre programas completos, amplitudes, fases, errores de IR, exportación y síntesis de varias familias aritméticas sin tablas. Su aporte comprobable es una arquitectura funcional y auditable sobre el subconjunto declarado.

Los costos observados sitúan división y resto entre las operaciones más exigentes del experimento, tanto en puertas como en workspace. Los límites de simulación impiden ejecutar varios de los circuitos que el backend sí puede construir. No se extrae de estos resultados una ventaja computacional práctica ni una certificación de corrección general.

El desarrollo pendiente se concentra en fortalecer control de flujo y vidas, completar ABI y composición entre unidades, ampliar síntesis dentro de presupuestos definidos y establecer una política de precisión. Una evaluación futura debería sumar versiones de GCC, campañas más extensas, costos después de descomposición y tiempos medidos con cargas comparables.

### 13.1 Repetir la evaluación del proyecto

En Linux o WSL, con GCC 11 y sus headers de plugin compatibles:

```bash
export CC=gcc-11
export CXX=g++-11
bash docs/paper/audit_project.sh
```

El script reconstruye el proyecto y ejecuta pruebas nativas, ejemplos, compilaciones diferenciales, crecimiento sin tablas y QuEST. La primera construcción de las herramientas de validación puede descargar la versión fijada de QuEST; el archivo y su checksum están especificados en `scripts/build-validation.sh`.

### 13.2 Inspeccionar un programa y sus recursos

```bash
build/cket examples/find.c --run --seed 7 -o build/find
build/cket examples/find.c --emit-ir -o build/find.qir.json
build/cket examples/find.c --emit-qasm -o build/find.qasm
build/cket_backend --validate build/find.qir.json
build/cket_backend --resources build/find.qir.json --no-tables
```

### 13.3 Regenerar este manuscrito

`docs/paper/c_ket_paper.md` es la fuente editable; `build_paper.py` produce el PDF y las siete figuras originales a partir de los datos incluidos. `audit_paper.py` verifica estructura, cifras, referencias, fuentes locales y geometría del documento. Los PNG de páginas son intermediarios de revisión visual y no sustituyen el PDF final. Las versiones de las herramientas editoriales y los hashes de los artefactos quedan registrados junto a la evidencia.

**Disponibilidad y atribución.** El código se identifica en el README mediante el repositorio `igna-s/cket`, indicado allí como privado; su acceso externo depende de las credenciales del lector. La revisión utilizó el checkout local. La licencia del proyecto es MIT y atribuye copyright a Ignacio Schwindt y colaboradores [1].

<!-- page -->

## Referencias

[1] Schwindt, I., y colaboradores. *|c> / cket: C with high-level quantum data*. Versión 0.4.0 experimental, commit `1ebb2e308e61bed5632798ded5beecf77b6b9a9c`, 2026. Código y documentación consultados localmente: `README.md`, `docs/language.md`, `docs/ir-verification.md`, `docs/circuit-abi.md`, `docs/c-coverage.md`, `docs/v1-plan.md`, `src/` y `tests/`. Evidencia reproducida: `docs/paper/evidence/`. [Repositorio identificado por el proyecto](https://github.com/igna-s/cket).

[2] Free Software Foundation. *GNU Compiler Collection Internals: Plugin API*. Documentación oficial; consulta: 11 de octubre de 2026. [API de plugins de GCC](https://gcc.gnu.org/onlinedocs/gccint/Plugin-API.html).

[3] Bennett, C. H. "Logical Reversibility of Computation". *IBM Journal of Research and Development*, 17(6), 525-532, 1973. DOI: 10.1147/rd.176.0525. [Artículo original](https://doi.org/10.1147/rd.176.0525).

[4] Grover, L. K. "A fast quantum mechanical algorithm for database search". *Proceedings of STOC*, 212-219, 1996. arXiv:quant-ph/9605043. [Preprint del autor](https://arxiv.org/abs/quant-ph/9605043).

[5] Boyer, M., Brassard, G., Høyer, P., y Tapp, A. "Tight bounds on quantum searching". *Fortschritte der Physik*, 46, 1998; preprint de 1996, arXiv:quant-ph/9605034. [Preprint de los autores](https://arxiv.org/abs/quant-ph/9605034).

[6] Bichsel, B., Baader, M., Gehr, T., y Vechev, M. "Silq: A High-Level Quantum Language with Safe Uncomputation and Intuitive Semantics". *PLDI*, 286-300, 2020. DOI: 10.1145/3385412.3386007. [Versión de los autores con material ampliado](https://files.sri.inf.ethz.ch/website/papers/pldi20-silq.pdf).

[7] Green, A. S., Lumsdaine, P. L., Ross, N. J., Selinger, P., y Valiron, B. "Quipper: A Scalable Quantum Programming Language". *ACM SIGPLAN Notices*, 48(6), 333-342, 2013. DOI: 10.1145/2499370.2462177. [Preprint de los autores](https://arxiv.org/abs/1304.3390).

[8] Cross, A. W., et al. "OpenQASM 3: A broader and deeper quantum assembly language". *ACM Transactions on Quantum Computing*, 3(3), 2022. DOI: 10.1145/3505636. [Preprint de los autores](https://arxiv.org/abs/2104.14722).

[9] OpenQASM. *OpenQASM 3.0 Specification: Gates*. Especificación oficial; consulta: 11 de octubre de 2026. [Controles, modificadores y fase global](https://openqasm.com/versions/3.0/language/gates.html).

[10] Jones, T., Brown, A., Bush, I., y Benjamin, S. C. "QuEST and High Performance Simulation of Quantum Computers". *Scientific Reports*, 9, 10736, 2019. DOI: 10.1038/s41598-019-47174-9. [Preprint de los autores](https://arxiv.org/abs/1802.08032).
