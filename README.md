# `|c>`: C con datos cuánticos de alto nivel

`cket` es un **prototipo funcional de compilador de C11 extendido con `quant`**. Utiliza el frontend real de GCC, agrega semántica cuántica a las variables calificadas, genera una Quantum IR propia y ejecuta los circuitos con un motor nativo de amplitudes escrito en C.

Este README contiene el informe técnico del estado implementado, su funcionamiento, las pruebas realizadas y los puntos propuestos para auditoría. Las propuestas se distinguen de las capacidades existentes.

Tres precisiones centrales:

- **El frontend es GCC extendido mediante un plugin.** Todavía no se modificó directamente el núcleo de tipos de GCC.
- **La ejecución actual es una simulación cuántica local en CPU.** Se exporta OpenQASM 3, pero todavía no se ejecuta en una QPU.
- **La cobertura cuántica de C sigue siendo parcial.** Se rechazan las construcciones sin una transformación implementada o cuya reversibilidad, propiedad o limpieza no se puede demostrar.

El programador utiliza operaciones de alto nivel:

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant uint8_t x = superposition(0, 15);
    quant uint8_t answer = find(x, x * x == 9);

    printf("%u\n", (unsigned)measure(answer));
    return 0;
}
```

El compilador genera el oráculo, la amplificación de amplitud y la limpieza del auxiliar. El programa fuente no escribe puertas ni gestiona ancillas.

El estado registrado corresponde al driver **0.4.0**, probado con **GCC 11.4.0**: **353 pruebas aprobadas y cero fallos** tanto en la suite normal como en la pasada de toda la cadena propia con AddressSanitizer y UndefinedBehaviorSanitizer.

| Capacidad | Estado actual |
|---|---|
| Enteros, booleanos y expresiones cuánticas | Implementados dentro de los límites de síntesis y simulación. |
| Funciones, arrays, estructuras y punteros | Funciones con resultados escalares propios, módulos QIR unitarios, productos fijos y préstamos comprobados. |
| Lecturas y escrituras de vectores con índice CPU | Operaciones indexadas admitidas, límites comprobados y medición explícita para obtener un resultado CPU. |
| Control e interferencia | `if/else` coherente, fases parametrizadas, Fourier/inversa y `find` experimental. |
| Ejecución y exportación | Simulación CPU nativa y exportación estática OpenQASM 3 para los programas admitidos. |
| Cobertura completa de C sobre datos `quant` | Pendiente; la [matriz de C11](docs/c-coverage.md) describe las restricciones y los trabajos restantes. |

Para probar el demo en Linux o WSL desde la raíz del proyecto:

```bash
bash scripts/build.sh
build/cket examples/find.c --run --seed 7 -o build/demo
```

Con esa semilla, el demo imprime `3`. La sección 17 contiene los requisitos, la exportación y los comandos de verificación.

## 1. Frontend real de C y componentes

El compilador no tiene un parser propio de C. GCC procesa los `#include`, macros, declaraciones, expresiones, promociones de enteros, funciones y estructuras del programa.

El plugin interviene sobre el AST real de GCC mediante `PLUGIN_PRE_GENERICIZE`, antes de que este se transforme en la representación genérica usada por las siguientes etapas del compilador.

A partir de ese AST identifica:

- Variables clásicas y cuánticas.
- Expresiones que dependen de datos cuánticos.
- Operaciones reversibles y asignaciones problemáticas.
- Condiciones clásicas o coherentes.
- Mediciones explícitas.
- Dependencias necesarias para descomputar temporales.

La parte clásica continúa compilándose como código máquina para CPU. Los punteros, estructuras y funciones puramente clásicos conservan la semántica de C.

| Archivo | Responsabilidad |
|---|---|
| [src/cket.c](src/cket.c) | Driver nativo: ejecuta GCC, valida la IR, exporta resultados y enlaza el ejecutable. |
| [src/cket_plugin.cpp](src/cket_plugin.cpp) | Integración con GCC, atributos, declaraciones y registro de tipos semánticos. |
| [src/quantum_semantics.inc](src/quantum_semantics.inc) | Análisis de expresiones, propiedad, efectos, control coherente y limpieza automática. |
| [src/quantum_frontend.inc](src/quantum_frontend.inc) | Composición de funciones, productos fijos, préstamos por punteros y expansión de bucles acotados sobre árboles de GCC. |
| [include/quantum.h](include/quantum.h) | Sintaxis pública de `quant` e intrínsecos de alto nivel. |
| [native/quantum.c](native/quantum.c) | Síntesis reversible, registros, ancillas, Grover, exportación y recursos. |
| [native/structured.inc](native/structured.inc), [native/arithmetic.inc](native/arithmetic.inc) | Síntesis simbólica y circuitos aritméticos por registros, con presupuestos de complejidad. |
| [native/circuit.inc](native/circuit.inc) | ABI nativa de módulos unitarios, remapeo de préstamos, control e inversa. |
| [native/verify.c](native/verify.c) | Verificación independiente del esquema, tipos y pruebas de rango de la IR antes de sintetizar o ejecutar. |
| [native/simulator.c](native/simulator.c) | Evolución del vector de amplitudes, medición y comprobación de limpieza. |
| [native/json.c](native/json.c), [native/json.h](native/json.h) | Parser de la IR serializada en JSON. **No analiza C.** |
| [native/quantum.h](native/quantum.h), [native/internal.h](native/internal.h) | API del motor y estructuras internas de registros, puertas y contexto. |
| [runtime/cket_runtime.c](runtime/cket_runtime.c) | Puente entre el ejecutable CPU y el motor cuántico enlazado. |
| [backend/cket_backend.c](backend/cket_backend.c) | Validación de IR, exportación QASM, recursos e inspección de amplitudes. |
| [tests/test_native.c](tests/test_native.c) | Pruebas de compilación, ejecución, amplitudes y rechazos semánticos. |
| [scripts/build.sh](scripts/build.sh) | Construcción del plugin, biblioteca, driver, backend y pruebas. |
| [scripts/test.sh](scripts/test.sh), [scripts/sanitize.sh](scripts/sanitize.sh), [scripts/sanitize-all.sh](scripts/sanitize-all.sh) | Pruebas normales, del núcleo instrumentado y de toda la cadena propia instrumentada. |
| [scripts/demo.sh](scripts/demo.sh) | Generación de los artefactos y ejecución del demo. |
| [scripts/unpack-gcc-deps.sh](scripts/unpack-gcc-deps.sh) | Extracción local de cabeceras de desarrollo de GCC cuando corresponde. |
| [docs/language.md](docs/language.md) | Documentación complementaria de tipos, propiedad y efectos. |
| [docs/ir-verification.md](docs/ir-verification.md) | Contrato del verificador y certificados de descomputación. |
| [docs/v1-plan.md](docs/v1-plan.md) | Especificación objetivo cerrada de 1.0 y criterios verificables para los siete requisitos. |
| [docs/c-coverage.md](docs/c-coverage.md) | Cobertura actual de C11, contratos de memoria y trabajo necesario para ampliar todo el lenguaje. |
| [examples/](examples/) | Programas C de preparación, tipos, control coherente, búsqueda y bucles híbridos. |
| [cket](cket) | Lanzador shell opcional del driver nativo `build/cket`. |

El driver, runtime, síntesis, simulador, parser de IR y pruebas están escritos en **C**. El plugin y su pasada semántica están escritos en **C++17**, porque utilizan la API interna de GCC.

El compilador y el runtime no dependen de Python ni de Qiskit Aer. La validación independiente opcional sí los utiliza. Se conserva el componente C++ del frontend y scripts shell de construcción; no se presenta todo el proyecto como C puro.

## 2. Integración de `quant` con los tipos de C

El encabezado implementa:

```c
#define quant __attribute__((cket_quant))
#define cuant quant
#define Cuant quant
```

El atributo `cket_quant` está registrado y es reconocido por el plugin de GCC. Actualmente `quant` es una macro que expande a un atributo de declaración; todavía no es una palabra reservada o un calificador nativo de la gramática de GCC.

Esto permite escribir:

```c
int classical = 10;

quant int quantum_integer = 10;
quant uint8_t x = superposition(0, 255);
quant bool flag = (x == 42);
```

La información semántica incluye el tipo C, anchura, signo, identidad del registro, propietario y dependencias:

```text
SemanticType = { GCC_C_Type, quantum, integer_width, signedness }
```

| Declaración | Interpretación |
|---|---|
| `int x` | Entero clásico de CPU. |
| `quant int x` | Registro cuántico con la anchura del `int` del destino. |
| `quant bool x` | Registro de un qubit. |
| `quant uint8_t x` | Registro de ocho qubits. |
| `quant uint16_t x` | Registro de dieciséis qubits. |

**La distinción entre `int` y `quant int` existe en el análisis semántico de `|c>` y en su IR. Todavía no son dos tipos cuántico/clásico nativos dentro del sistema central de tipos de GCC.**

El plugin ya admite parámetros prestados, resultados de expresiones puras, arrays fijos y estructuras enteras. La ABI de circuitos QIR ya permite módulos unitarios independientes. La propagación nativa del calificador y el enlace de funciones C `quant` arbitrarias entre unidades siguen pendientes.

El plugin añade una marca interna `volatile` para impedir que algunas simplificaciones tempranas oculten accesos cuánticos, como `x*0` o `x-x`. Es una solución provisional; no integra por sí sola la semántica cuántica en todas las optimizaciones de GCC.

Se admiten enteros y booleanos de hasta 64 bits, arrays fijos multidimensionales y estructuras cuyos campos sean esos tipos. Las funciones cuánticas se expanden en su llamador sobre el AST de GCC; sus parámetros prestan registros, incluidos punteros y parámetros de arrays con almacenamiento comprobado. Los propietarios originales siguen siendo locales automáticos de `main`, incluidas las variables de procedimientos expandidos. Se rechazan almacenamiento global, VLA, unions, bitfields y tipos de coma flotante cuánticos.

Un puntero como `quant uint8_t *p = &x` es una referencia con dirección clásica al registro existente. Sus dereferencias y desplazamientos constantes se resuelven con límites comprobados; no se prepara una superposición de direcciones ni se copia el estado. Las referencias no pueden escapar a memoria CPU ni reasignarse dinámicamente. Los préstamos de solo lectura y los solapamientos de parámetros mutables se comprueban.

Los arrays fijos escalares y sus préstamos también admiten índices CPU calculados durante la ejecución. Por ejemplo, `values[index] += 2` selecciona un registro y lo actualiza sin medirlo. Se admiten suma/resta y XOR de enteros sin signo con operandos CPU, incrementos, medición/reset explícitos y Fourier/inversa; se comprueban los límites y la evaluación única de operandos. La selección se conserva en los bucles CPU y requiere ejecución nativa. Una lectura cuántica hacia CPU sigue escribiéndose `measure(values[index])`; medir un elemento elegido no demuestra que todos los propietarios hayan sido consumidos. Los índices cuánticos, accesos dinámicos a productos anidados y cálculos cuánticos generales con elementos seleccionados siguen pendientes. [dynamic_vector.c](examples/dynamic_vector.c) muestra una entrada por `scanf`, actualización y salida medidas.

Por ejemplo, se admite:

```c
void accumulate(quant uint8_t values[2]) {
    values[0] += values[1];
}

quant bool is_solution(quant const uint8_t value) {
    return value * value == 9;
}
```

Las definiciones deben preceder sus usos en la misma unidad de traducción. Los resultados admiten expresiones puras con ramas condicionales o un cuerpo con temporales y retorno terminal escalar propio, transferido a un inicializador `quant` nuevo. Los procedimientos `void` conservan su `return;` final. Se rechazan retornos tempranos con vida pendiente, recursión y punteros a funciones cuánticas. El préstamo de un parámetro no puede consumirse mediante un movimiento o `find`.

Este es el programa completo de [examples/dynamic_vector.c](examples/dynamic_vector.c):

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    unsigned index;
    if (scanf("%u", &index) != 1 || index >= 2) return 1;

    quant uint8_t values[2] = {1, 7};
    values[index] += 2;
    printf("%u\n", (unsigned)measure(values[index]));

    reset(values[0]);
    reset(values[1]);
    return 0;
}
```

`scanf` recibe un índice clásico. La actualización actúa sobre el registro elegido, y `measure` produce el valor que imprime `printf`. Con entrada `0` imprime `3`; con entrada `1` imprime `9`. Los resets explícitos dan un final de vida válido a ambos propietarios, incluido el elemento que no se seleccionó. Pasar directamente almacenamiento cuántico a `scanf` o `printf` no tiene una conversión implícita admitida.

En una actualización como `values[index] = values[index] + amount`, se exige que ambos accesos identifiquen el mismo elemento y que el operando sea clásico, dentro del camino admitido de enteros sin signo. Se comprueban las conversiones y se calcula el desplazamiento modular sin introducir desbordamientos con signo al reorganizarlo. Se rechaza sustituir el acceso derecho por otro elemento o tratar una conversión que pierde bits del destino como si fuera esa suma reversible. Las mediciones usadas como índice u operando se ejecutan antes de actualizar y de liberar los registros utilizados por esos operandos.

Una declaración cuántica sin inicializador se prepara actualmente en cero:

```c
quant uint8_t x;
```

Eso significa `|0>` en este prototipo. Es una decisión del lenguaje que debe documentarse expresamente: un entero automático clásico de C sin inicializar no tiene esa semántica.

Los módulos unitarios independientes se componen con `circuit("module.qir.json", x)` e `inverse_circuit`. Se incorporan al compilar, verifican préstamos por representación y conservan fases globales al controlarlos. [circuit-abi.md](docs/circuit-abi.md) describe la ABI y [circuit_module.c](examples/circuit_module.c) la utiliza.

## 3. Arquitectura híbrida CPU/cuántica

El recorrido actual es:

```text
Archivo C + quant
        |
        v
Frontend real de GCC
        |
        v
AST + análisis semántico cuántico
        |
        +-- Código CPU con llamadas al runtime
        |       |
        |       v
        |   Objeto y ejecutable nativo
        |
        +-- Quantum IR tipada
                |
                v
        Validación y síntesis reversible
                |
                +-- OpenQASM 3
                +-- Estimación de recursos
```

Durante la ejecución:

```text
Código máquina CPU
        |
        v
Runtime C enlazado
        |
        v
Operación Quantum IR -> síntesis de puertas en C
        |
        v
Evolución del vector de amplitudes
        |
        v
measure() -> valor clásico -> continúa el programa CPU
```

**El ejecutable contiene operaciones cuánticas serializadas en JSON, pero ya conserva documentos parseados y planes de puertas reutilizables.** La verificación de tipos, vidas y certificados se mantiene en cada llamada. La caché evita volver a parsear operaciones repetidas y evita sintetizar de nuevo expresiones y sumas con las mismas precondiciones, cables y controles.

Los operandos CPU parametrizan variantes de suma, fase y selección indexada. Los módulos unitarios se validan/compilan una vez y se reutilizan. Hay límites de 16 MiB de documentos parseados, 64 MiB de planes y 64 MiB de módulos; un plan con trabajo privado solo se reutiliza si sus cables están libres.

Todavía no se enlaza de forma general una tabla definitiva de puertas sintetizada durante la compilación. La primera llamada puede sintetizar; las plantillas parametrizadas generales siguen pendientes. La regresión de ocho cálculos y sus inversas comprueba cinco parses, una síntesis y quince aciertos de plan.

El motor se ejecuta dentro del mismo proceso del programa, enlazado con `libcket_quantum.a`. No inicia Python ni un proceso externo para realizar las cuentas cuánticas.

El plugin reemplaza operaciones por llamadas como `__cket_apply`, `__cket_apply_arg`, `__cket_apply_phase`, `__cket_apply_index` y `__cket_measure`. Las variantes transportan operandos enteros, ángulos clásicos o índices CPU hacia el motor. Las variables CPU utilizadas como marcadores de almacenamiento cuántico no contienen una lectura clásica del estado cuántico.

## 4. Preparación y medición: primera prueba de concepto

Este programa está dentro del alcance implementado:

```c
#include <stdint.h>
#include <stdio.h>
#include <quantum.h>

int main(void) {
    int classical = 10;

    quant uint8_t x = superposition(0, 255);
    uint8_t result = measure(x);

    printf("%d %u\n", classical, (unsigned)result);
    return 0;
}
```

Sus instrucciones cuánticas son:

```json
[
  {
    "op": "qalloc",
    "id": 0,
    "name": "x",
    "width": 8,
    "signed": false,
    "ancilla": false
  },
  {
    "op": "qprepare",
    "id": 0,
    "lower": 0,
    "upper": 255
  },
  {
    "op": "qmeasure",
    "id": 0,
    "result": "m0",
    "width": 8
  }
]
```

La preparación genera internamente las operaciones necesarias para representar:

```text
|psi> = (1/16) * sum(j = 0..255) |j>
```

Cada resultado tiene probabilidad `1/256`. El programador no escribe las puertas de preparación.

`measure(x)` toma una muestra, colapsa explícitamente el registro y devuelve el resultado a C. Después se ejecuta el `printf` clásico. La macro de medición conserva el tipo C del registro al devolver el resultado.

Las preparaciones admitidas son:

- Un valor de base, como `quant uint8_t x = 7`.
- Un intervalo uniforme `[0, 2^k-1]`.

Todavía se rechazan intervalos uniformes arbitrarios y límites calculados dinámicamente por la CPU. Un valor de base sí puede calcularse durante la ejecución: `quant uint8_t x = read_value();` evalúa el inicializador CPU una vez y prepara almacenamiento nuevo, conservando su conversión C. La exportación estática se rechaza cuando no conoce ese valor.

Los intrínsecos públicos incluyen `superposition`, `measure`, `reset`, `phase_flip`, `phase`, `fourier`, `inverse_fourier`, `find`, `find_known`, `search`, `circuit` e `inverse_circuit`.

`phase(theta)` aplica una fase de ángulo clásico finito a los controles activos. Sin controles es global. Un ángulo calculado por CPU se evalúa una vez y requiere ejecución nativa; su exportación estática se rechaza. `fourier(x)` implementa la DFT cuántica positiva de todo el registro en orden little-endian; `inverse_fourier(x)` invierte esa transformación. Admiten controles coherentes, requieren almacenamiento sin inversas pendientes y se sintetizan con coste lógico cuadrático en la anchura, sin tablas de entrada. Las amplitudes se comparan contra la matriz DFT independiente en las pruebas.

## 5. Expresiones, promociones y conversiones de C

Están implementadas expresiones cuánticas puras con:

```text
+  -  *  /  %
== != < <= > >=
&  |  ^  ~
<< >>
!  && ||
?:
```

Cada nodo de la expresión IR conserva anchura y signo. Esto es fundamental para:

```c
quant uint8_t x = superposition(0, 255);
quant bool flag = (x * x == 9);
```

En C, `uint8_t` se promociona a `int` para esa multiplicación. El compilador conserva esa promoción: no convierte silenciosamente el producto en una multiplicación modular de ocho bits.

La prueba correspondiente verifica que se marque `3` y que no aparezca `253` como una falsa raíz causada por truncar el cuadrado a ocho bits.

Una conversión explícita o la conversión al tipo del resultado sí aplica la anchura correspondiente.

También se contemplan:

- División con signo truncada hacia cero.
- Resto con el comportamiento aritmético de C.
- Conversión a `bool` mediante comparación con cero.
- Evaluación condicional de `&&`, `||` y `?:` cuando sus componentes son puros.
- Detección de división por cero, desplazamientos inválidos y desbordamientos con signo encontrados durante la síntesis.

Por ejemplo, se prueba:

```c
quant uint8_t x = superposition(0, 3);
quant uint8_t y = x ? 8 / x : 7;
```

La rama de división no se evalúa para `x == 0`.

La evaluación aritmética de la síntesis utiliza la extensión GCC `__int128` para manejar y comprobar operaciones de enteros de hasta 64 bits. Las conversiones con signo se corresponden con el destino GCC probado.

**El sistema no demuestra la ausencia de todo comportamiento indefinido en C arbitrario**, especialmente cuando GCC haya transformado previamente una expresión.

La existencia de un operador en esta lista no implica que todas sus formas de asignación, combinación con efectos CPU o usos en cualquier contexto C estén implementados.

## 6. Asignaciones, reversibilidad y no clonación

Existen tres casos principales.

### Actualización modular reversible

```c
x += 5;
```

Para un registro sin signo de anchura `w` representa:

```text
|x> -> |x + 5 mod 2^w>
```

Es reversible porque es una permutación. También se soportan resta e incrementos/decrementos en los contextos implementados.

Las sumas y restas entre registros sin signo distintos, `x += y` y `x -= y`, conservan `y` y utilizan síntesis directa sin ancillas ni tablas exhaustivas. `x ^= y`, XOR de expresiones admitidas y XOR con operandos CPU también conservan la información; los operandos CPU se evalúan una vez. Las conversiones booleanas necesitan una prueba adicional y no se tratan como XOR modular de un bit indiscriminadamente.

Los tipos booleanos necesitan sus reglas de conversión: `x = !x` es una permutación, mientras que `x += 1` sobre `bool` no lo es y se rechaza.

### Cálculo en un resultado nuevo

```c
quant uint8_t y = x % 2;
```

Se calcula conservando `x`:

```text
|x>|0> -> |x>|x % 2>
```

La operación reversible general utiliza un acumulador XOR:

```text
|x>|y> -> |x>|y XOR f(x)>
```

El resultado puede quedar entrelazado con su entrada. No se crea una copia independiente de un estado desconocido.

También se admite calcular en almacenamiento inicialmente cero en los contextos restringidos implementados:

```c
quant uint8_t y;
y = x % 2;
```

En cambio:

```c
x = x % 2;
```

se rechaza porque sobrescribe el único registro y pierde información.

### Transferencia de propiedad

```c
quant uint8_t b = a;
```

Tiene actualmente semántica de movimiento:

- `b` pasa a ser propietario del registro existente.
- No se asigna un segundo registro.
- `a` queda consumido.
- Un uso posterior de `a` produce un diagnóstico.

Esta es una modificación consciente de la semántica de asignación para datos cuánticos. El compilador no intenta clonar el estado.

El movimiento y el resultado de `find` requieren el mismo tipo C canónico. Los alias `typedef` compatibles se admiten; tener la misma anchura y signo no basta para mover entre tipos C distintos, como `char` y `signed char`. Un inicializador que cambia anchura o signo calcula una conversión de valor y conserva la entrada, incluso cuando GCC lo representa mediante un `NOP_EXPR`.

No existe todavía una solución general para cualquier asignación `b = a` sobre un destino ya ocupado.

Para otras actualizaciones del propio registro, como:

```c
x = x * 3 + 1;
```

el backend comprueba que la transformación sea inyectiva sobre **todo el espacio de bits del registro**, tras aplicar las conversiones pertinentes. Si forma una permutación, puede sintetizarla.

Esta comprobación general está limitada a registros de hasta 12 bits. Los caminos especializados de suma modular no tienen ese mismo límite de síntesis por tabla, aunque siguen sujetos a los límites de almacenamiento y simulación.

El tratamiento de asignaciones destructivas preservando automáticamente todo un historial de valores anteriores sigue pendiente.

## 7. `if/else` cuántico coherente

Por ejemplo:

```c
quant bool c = superposition(0, 1);
quant uint8_t x = 0;

if (c) {
    x += 1;
} else {
    x += 2;
}
```

La transformación conceptual es:

```text
alpha|0>|x> + beta|1>|x>
    ->
alpha|0> U_else|x> + beta|1> U_then|x>
```

No se mide `c` ni se selecciona una única rama clásica.

La implementación:

1. Calcula la condición en un auxiliar.
2. Controla la rama `then` con el auxiliar en uno.
3. Controla la rama `else` con el auxiliar en cero.
4. Descomputa la condición.
5. Libera el auxiliar limpio.

Se soportan condiciones cuánticas anidadas acumulando controles.

Dentro de esas ramas se rechazan efectos que no tienen una transformación coherente implementada: I/O, llamadas clásicas, escrituras CPU, medición, reset y saltos clásicos. Esto incluye `n++`, `--n`, efectos CPU en los argumentos de procedimientos expandidos y ensamblador en línea; no se ejecutan incondicionalmente al traducir el `if` cuántico. Los procedimientos cuánticos admitidos sí pueden componerse bajo esos controles cuando cumplen sus contratos de efectos y préstamos.

También se restringen las declaraciones cuánticas dentro de las ramas porque todavía falta una prueba general de sus tiempos de vida.

Hay un caso que exige distinguir pérdida de información de las restricciones conservadoras del análisis:

```c
if (x == 0) {
    x += 1;
}
```

Como transformación solo sobre `x`, envía tanto `0` como `1` a `1`. No es inyectiva y no puede implementarse como una operación unitaria sobre ese único registro. Se rechaza por ese conflicto semántico, además de modificar una entrada necesaria para invertir la condición.

Conservar la condición o el valor anterior permitiría distinguir las entradas, pero no garantiza que ese historial pueda limpiarse manteniendo el resultado pretendido. La descomputación automática tiene condiciones específicas. Otros cambios de entradas pueden ser reversibles, pero también necesitan una prueba que el análisis actual no siempre proporciona.

Un flag independiente sí puede controlar una actualización de otro registro.

`phase_flip()` aplica una fase de pi a los estados seleccionados por los controles activos. Fuera de un control representa una fase global y no cambia por sí sola las probabilidades de medición.

## 8. Bucles y control clásico

Este ejemplo funciona:

```c
quant uint8_t x = 1;

for (int i = 0; i < 8; ++i) {
    x += i;
}

printf("%u\n", (unsigned)measure(x));
```

El resultado es `29`.

GCC genera el bucle CPU y cada iteración ejecuta la actualización cuántica correspondiente. También hay pruebas de `while` y `do` clásicos.

Se conserva la evaluación única de operandos con efectos en ejemplos como:

```c
x += ++n;
x += measure(y);
```

Fuera de control cuántico coherente, esos operandos se evalúan una sola vez antes de la suma. El plugin conserva los `SAVE_EXPR` compartidos generados por GCC para esos operandos.

Actualmente se rechazan:

- Bucles cuya condición sea un valor cuántico.
- Bucles dentro de control cuántico coherente.
- Nuevas asignaciones de almacenamiento cuántico en flujo CPU dinámico. Se admiten medidas/reset sobre propietarios estables; un camino que puede no ejecutarse no demuestra consumo o limpieza en los demás caminos.
- Ramas clásicas anidadas dentro del control cuántico coherente que todavía no tienen una transformación implementada.

Se admiten `if` coherentes dentro de bucles CPU con propietarios estables: los auxiliares se asignan y limpian en cada iteración. Las uniones de ramas y aristas de retorno conservan solo hechos demostrados en todos los caminos. Un bucle que puede ejecutar cero iteraciones no demuestra haber medido un registro. Las vidas dinámicas generales y el CFG completo siguen pendientes.

Existe además una excepción comprobada para `for` con contador local, límites constantes, comparaciones y paso `++`/`--`: se expanden hasta 4096 iteraciones cuando permiten accesos a productos, temporales o control coherente. Esto admite lecturas, mediciones y escrituras de vectores con índices estáticos. No se expanden bucles con saltos, cambios del contador en el cuerpo o una cota que dependa de desbordarlo. Un bucle CPU cuyo contador se vuelve a cero conserva su ejecución real; no se inventa una terminación.

Un `while` cuántico general requeriría definir un límite y una representación reversible del avance y del historial. Eso no está implementado.

La IR marca las regiones CPU dinámicas. El ejecutable híbrido puede conservar las construcciones admitidas, pero su exportación como un circuito QASM plano se rechaza: aplanarlas perdería la ejecución real del control CPU.

## 9. Descomputación automática y limpieza

Para cada resultado derivado se registran:

- Su expresión creadora.
- Los registros de entrada.
- Los controles externos normalizados y su patrón.
- Las versiones de las entradas y de los controles.
- Los usos restantes.
- Su estado de propiedad y limpieza.

En:

```c
quant bool flag = (x + 5 == 42);

if (flag) {
    phase_flip();
}
```

Se calcula el predicado, se utiliza como control y se elimina mediante su operación inversa cuando ya no se necesita.

La síntesis actual puede construir directamente la función booleana completa. No necesita asignar un registro separado para cada nodo, como `x+5`.

Cuando existen registros temporales, la IR expresa su limpieza mediante:

```text
quncompute
qrelease
```

La eliminación respeta las dependencias: primero se limpian los resultados que dependen de otros valores.

El backend exige la misma expresión creadora, la misma máscara y patrón de control y dependencias disponibles con las mismas versiones antes de aceptar una inversa. Reordenar controles equivalentes se admite. Por ahora la expresión se compara por su texto JSON; una serialización distinta pero equivalente puede rechazarse.

Mientras un resultado necesita una entrada o control para su limpieza, se rechaza actualizar, medir, resetear o liberar esa dependencia. Una fase diagonal puede conservar la prueba porque no modifica los valores de base utilizados por el cálculo; sí invalida, cuando corresponde, la uniformidad necesaria para `find`. Las pruebas comprueban que esas fases relativas sobrevivan a una descomputación controlada.

En simulación, liberar un registro también exige que la probabilidad de encontrarlo fuera de cero sea menor que la tolerancia numérica de `1e-10`.

No se insertan mediciones o resets para hacer posible una limpieza que no se pudo demostrar.

Antes de salir, los registros deben tener un tratamiento de vida aceptado: medición/reset explícitos, transferencia a un propietario que continúe gestionándolos o descomputación y liberación cuando corresponda. Se rechazan salidas cuya limpieza no se pueda demostrar.

Un registro medido no se considera automáticamente almacenamiento limpio para reutilizar. Una medición explícita puede dejar bits en uno; el reset explícito permite volverlos a cero.

El análisis sigue siendo conservador, basado en recorridos del AST, usos y dependencias. Todavía no constituye un análisis completo de flujo de control, préstamos y propiedad entre funciones.

## 10. Quantum IR y representación de puertas

El formato se identifica como:

```text
format: cket.qir
version: 1
frontend: GCC C
```

Incluye símbolos, expresiones tipadas e instrucciones:

| Instrucción | Función |
|---|---|
| `qalloc` | Asignar almacenamiento inicialmente limpio. |
| `qprepare` | Preparar un estado de base o uniforme. |
| `qcompute` | Calcular reversiblemente una expresión en otro registro. |
| `quncompute` | Invertir el cálculo de un resultado derivado. |
| `qrelease` | Liberar almacenamiento probado cero. |
| `qadd_inplace` | Actualización modular reversible. |
| `qadd_register` | Suma o resta entre registros sin signo distintos. |
| `qxor_inplace` | Acumulación XOR reversible con una expresión o un operando clásico. |
| `qinplace` | Aplicación de una permutación sintetizada. |
| `qphase` | Fase global o controlada. |
| `qfourier` | Transformada de Fourier o su inversa, con controles coherentes. |
| `qindexed` | Selección CPU de un registro existente para una operación indexada admitida. |
| `qmeasure` | Medición explícita. |
| `qreset` | Reset explícito. |
| `qmove` | Transferencia de propiedad. |
| `qfind` | Búsqueda mediante amplificación de amplitud. |
| `qsearch` | Intentos con cantidad desconocida y candidato medido verificado. |
| `qborrow` | Parámetro tipado de un módulo independiente. |
| `qmodule` | Aplicación controlada o inversa de un módulo unitario. |
| `cloop_begin/end`, `ccontrol_begin/end` | Marcas de control CPU dinámico. |

Los operadores también aparecen como **nodos de expresión dentro de la IR**. Existen caminos directos para sumas, restas, XOR y Fourier, y síntesis simbólica para ciertas expresiones de bits y predicados. No se dispone todavía de síntesis escalable para toda la aritmética.

Después se produce una representación de puertas con operaciones internas de H, X, fase, medición y reset. Las X controladas se representan mediante máscaras y patrones de control.

El contexto también conserva los registros, las versiones, las definiciones y controles de resultados derivados y un conjunto de qubits limpios disponibles para reutilizar.

El verificador independiente comprueba campos admitidos y duplicados, anchuras, signo, aridades, conversiones explícitas, disponibilidad de registros, controles y ángulos finitos. Revisa estructuralmente todas las ramas de una expresión, aunque su aritmética solo se evalúa cuando corresponde por cortocircuito. Las operaciones de medición, reset, preparación y vida del almacenamiento rechazan controles coherentes.

Los documentos comprueban formato, versión y equilibrio de marcas de control CPU. Esto todavía no representa un grafo de control completo ni demuestra todos los caminos del programa. El contrato y los límites se describen en [Validación de la Quantum IR](docs/ir-verification.md).

Aunque el formato se llame `cket.qir`, **no es LLVM QIR**. Tampoco está implementada la salida `--emit-llvm`.

## 11. Síntesis estructurada y camino finito

El backend intenta primero sintetizar la expresión por su estructura. Los operadores de bits, conversiones y desplazamientos constantes admitidos se expresan mediante polinomios booleanos simbólicos sobre los cables de entrada. Los predicados contra constantes, incluido el orden con signo cuando sus bits pueden representarse como literales, generan controles y prefijos directamente. Esta ruta no recorre las entradas y admite predicados sobre registros de 32 bits. Tiene un límite de 256 términos por bit y dos millones de pasos de trabajo global, incluyendo comparaciones de términos. El verificador limita las expresiones a 100.000 tokens y 128 niveles. Un presupuesto agotado produce un diagnóstico; una composición no cubierta puede usar el camino finito pequeño.

En el camino finito, el backend reúne los bits de entrada relevantes y evalúa la expresión tipada sobre su dominio.

Con hasta 12 bits de entrada existen como máximo `4096` combinaciones. Los bits conocidos cero y los valores de base conocidos pueden excluirse cuando el motor también dispone de una prueba de ese hecho. Ya no se aceptan `known` y `active` solo porque el frontend los haya emitido: el verificador los contrasta con el estado abstracto mantenido por el backend. Una actualización actualiza o descarta conservadoramente esa prueba.

A partir de la tabla resultante construye:

- Una representación por minterminos.
- Una representación en forma normal algebraica, mediante una transformación XOR.

La transformación algebraica aplica:

```text
anf = tabla

para cada bit b:
    para cada índice i que contenga b:
        anf[i] ^= anf[i sin el bit b]
```

Elige entre ambas representaciones con una heurística basada en el número de términos que generan operaciones. Después emite X controladas sobre los bits correspondientes del resultado.

Esto sintetiza una operación reversible real sobre superposiciones. La evaluación de la tabla sirve para construir el circuito; no sustituye su ejecución por una respuesta clásica.

Hay dos caminos adicionales:

- **Sumas modulares con operandos clásicos:** utilizan una construcción directa mediante controles de acarreo.
- **Actualizaciones generales inyectivas:** enumeran la permutación, la descomponen en ciclos y generan intercambios entre estados de base mediante caminos que cambian un bit por paso.

Para más de 12 bits relevantes se incorporaron circuitos por registros para suma/resta, multiplicación, división, resto y comparaciones entre registros. Se admiten operandos negativos cuando se demuestra ausencia de overflow: la división opera sobre magnitudes, trunca hacia cero y restaura el signo; el resto conserva el signo del dividendo. El divisor debe probarse distinto de cero, por rango o por un bit siempre uno, y se rechaza `MIN/-1`. Se calcula en trabajo privado, se copia por XOR al resultado y se invierte el plan para limpiar el trabajo.

El backend admite `--no-tables` para exigir un camino estructurado también en dominios pequeños. `scripts/benchmark_synthesis.py` compila 24 circuitos a anchuras 4, 8, 12 y 16 y publica puertas, auxiliares y profundidad. Para multiplicación modular de 16 bits registra 1680 puertas lógicas y 16 auxiliares; dos entradas de 16 bits suman 32 bits relevantes. Son recursos antes de descomponer MCX.

La ruta finita conserva coste exponencial y permanece para casos pequeños, permutaciones y expresiones no cubiertas. Faltan pruebas de rango más precisas, composiciones generales y el enlace de todas las operaciones como circuitos estáticos. El divisor actual necesita un registro de trabajo de una anchura adicional y rechaza el caso de 64 bits. Las construcciones por incrementos tienen hasta coste cúbico en puertas lógicas y trabajo lineal para un operador binario; componerlos no elimina los límites de vida o espacio.

La heurística actual tampoco calcula el coste físico real de descomponer controles grandes en puertas de un dispositivo.

Durante la simulación las puertas se aplican inmediatamente y no se acumula una lista creciente de todas las puertas ejecutadas. En exportación sí se conserva la representación de puertas.

## 12. Motor nativo de amplitudes

El estado se almacena como:

```c
double complex *state;
```

Para `n` qubits contiene `2^n` amplitudes:

```text
|psi> = sum(i = 0..2^n-1) alpha_i |i>
```

El orden de bits es little-endian. El estado inicial tiene amplitud uno en cero y cero en los demás índices; ampliar el almacenamiento incorpora qubits nuevos en cero.

Las operaciones internas realizan las cuentas directamente en C:

- H transforma un par `(a,b)` en `((a+b)/sqrt(2), (a-b)/sqrt(2))`.
- X intercambia amplitudes.
- X controlada intercambia solamente amplitudes cuyo patrón de control coincide.
- Una fase multiplica amplitudes seleccionadas por `cos(theta) + I*sin(theta)`.

Las operaciones unitarias conservan las superposiciones y correlaciones sin tomar muestras.

### Medición y reset

La medición calcula probabilidades mediante `|alpha_i|^2`, toma una muestra y normaliza el estado compatible con el resultado.

Se mide cada bit del registro de forma secuencial, conservando las correlaciones con los demás registros. Repetir una medición sin operaciones intermedias devuelve el mismo resultado.

`reset(x)` implementa explícitamente una trayectoria de colapso y corrección para dejar el registro en cero. La destrucción de información está autorizada por esa operación escrita en el programa.

La semilla puede fijarse con `--seed` o `CKET_SEED`. El generador utilizado para muestreo es SplitMix64.

### Alcance del simulador

El simulador no implementa ruido, matrices de densidad, GPU ni conexión a hardware cuántico. El runtime utiliza actualmente un contexto global; la ejecución cuántica concurrente entre hilos necesita trabajo adicional.

La herramienta `--state` permite inspeccionar amplitudes para pruebas del simulador. Omite las mediciones del documento al construir esa vista de depuración; no es una operación disponible para que un programa cuántico lea amplitudes sin medir.

Los cálculos de amplitudes utilizan precisión finita de `double`. La exactitud lógica de una tabla reversible no equivale a aritmética compleja de precisión infinita en el simulador.

## 13. Búsqueda: oráculo, cantidad conocida y desconocida

El oráculo se construye sin contar soluciones. `find_known(x, predicate, M)` exige una promesa del llamador `1 <= M <= N`, elige iteraciones de Grover con esa cantidad y transfiere la propiedad del registro. `find(x, predicate)` es la abreviatura con **M=1**. Este contrato reemplaza el conteo del prototipo anterior; si hay varias soluciones hay que proporcionar la cantidad o utilizar `search`.

```c
quant uint8_t x = superposition(0, 15);
quant uint8_t answer = find_known(x, x * x == 9, 1);
printf("%u\n", (unsigned)measure(answer));
```

No se clona el estado: `answer` utiliza el registro de `x` y el nombre anterior queda consumido. Se exige entrada uniforme e intacta, predicado puro sobre esa entrada y ausencia de préstamos/dependencias incompatibles. El backend comprueba esa preparación de forma independiente.

Para cantidad conocida se utiliza `theta = asin(sqrt(M/N))` y `P(r)=sin²((2r+1)*theta)`, comparando los enteros próximos a `pi/(4*theta)-1/2`. No se comprueba la promesa enumerando. Con N=16 y M=1 hay tres iteraciones y probabilidad `0.9613189697265625`. Con M=4, el ejemplo `x%4==1` utiliza una iteración. Una promesa incorrecta no da una garantía válida.

Para cantidad desconocida, incluso cero:

```c
quant uint8_t x = superposition(0, 15);
bool found = search(x, x * x == 9, 80);
if (found) printf("%u\n", (unsigned)measure(x));
else reset(x);
```

`search` mezcla muestras directas e iteraciones aleatorias de Grover, mide y verifica cada candidato. Conserva el registro con el último candidato medido; entre intentos mide/resetea y prepara nuevamente la entrada. `true` siempre corresponde a un candidato válido. `false` significa **presupuesto agotado**, incluso cuando no hay soluciones; no es una prueba de inexistencia.

Bajo aleatoriedad uniforme independiente, si hay alguna solución la probabilidad de agotar t intentos es a lo sumo `(7/8)^t`. El runtime usa un PRNG reproducible, muestreo sin sesgo modular y presupuestos de 1 a 4096 intentos; la garantía no es determinista para una semilla particular. Se evalúan candidatos individuales, nunca todo el dominio para elegir la estrategia. Las regresiones cubren cero, una, varias y todas las soluciones, más la compilación de dominios de 16 bits. `search` requiere feedback CPU y no se exporta como un QASM plano.

El oráculo todavía está sujeto a los caminos y límites de síntesis de su predicado. Eliminar el conteo no convierte en escalable un predicado que solo tenga síntesis finita ni elimina los límites del simulador.

## 14. OpenQASM 3 y estimación de recursos

El exportador genera OpenQASM 3 con registros, puertas controladas, fases, mediciones y resets.

Después de generar el demo se dispone de:

- [AST de GCC](build/demo.ast).
- [Quantum IR](build/demo.qir.json).
- [OpenQASM 3](build/demo.qasm).
- [Recursos](build/demo.resources.txt).

Estos artefactos están dentro de `build/`, que se excluye del control de versiones. Los comandos de la sección 17 permiten regenerarlos.

El informe del demo es:

```text
Logical qubits: 8
Ancillas:       1
Peak qubits:    9
H              28
X              24
MCX            6
PHASE          6
MEASURE        8
Depth:         26
```

Aunque el dominio sea `0..15`, `uint8_t` conserva un registro de ocho qubits. Cuatro participan en la superposición inicial y los superiores comienzan en cero.

Los conteos distinguen X, CNOT, Toffoli y controles mayores. También incluyen fases, mediciones y resets cuando aparecen. La profundidad se estima a partir de las dependencias entre operaciones sobre los mismos qubits.

**No incluye descomposición, conectividad, enrutamiento ni tiempos de un dispositivo.**

Los controles abiertos se cuentan como operaciones lógicas individuales y se expanden con X alrededor del control positivo al exportar. Por ejemplo, el QASM del demo contiene **48 X**, frente a las **24 X** del conteo lógico.

Ese informe no debe presentarse como coste final de hardware.

Los programas con control CPU dinámico que no pueda representarse en esta exportación estática se rechazan para `--emit-qasm` y el informe estático de recursos, aunque la ejecución híbrida nativa sea admitida.

## 15. Límites y capacidades pendientes

| Aspecto | Límite o restricción |
|---|---|
| Tipo entero cuántico | Hasta 64 bits. |
| Registros físicos de la IR de puertas | Hasta 128 qubits totales. |
| Simulación local | Hasta 24 qubits físicos totales. |
| Síntesis por tablas de expresiones | Hasta 12 bits de entrada relevantes; las rutas estructuradas no comparten este límite. |
| Permutaciones generales en el propio registro | Hasta 12 bits de anchura. |
| Preparación uniforme | `[0, 2^k-1]`, con límites constantes. |
| Ubicación de propietarios cuánticos | Locales de `main`, incluidas las variables de funciones expandidas. |
| Funciones y parámetros cuánticos | Resultados escalares propios, préstamos y módulos QIR unitarios independientes; enlace C entre unidades y retornos generales pendientes. |
| Arrays y estructuras cuánticos | Productos enteros fijos; operaciones indexadas por CPU en arrays escalares y acceso constante a productos anidados. |
| Punteros a datos cuánticos | Préstamos comprobados y operaciones indexadas por CPU; memoria dinámica y direcciones cuánticas pendientes. |
| Bucles cuánticos generales | Rechazados. |
| Asignación general a un destino ocupado | Pendiente. |
| Fases y cambios de base | `phase`, `fourier`, `inverse_fourier`; composición general y presupuesto de error pendientes. |
| QPU remota / LLVM QIR | Pendientes. |

**Aceptar un tipo no implica poder simularlo.** En el destino probado, `quant int` tiene 32 bits: puede representarse en la IR y exportarse, pero supera por sí solo el límite de 24 qubits del simulador.

La memoria del vector crece exponencialmente:

```text
memoria del vector = 2^n * sizeof(double complex)
```

Con amplitudes complejas de 16 bytes, 24 qubits requieren aproximadamente **256 MiB solamente para el vector**, además de las estructuras auxiliares.

Los qubits liberados se reutilizan después de probar que están en cero. El vector no reduce su dimensión cada vez que se libera un registro.

Existen además límites defensivos del prototipo, como 64 MiB para archivos de IR leídos, hasta 128 fuentes en una expresión y hasta 2.097.152 entradas de puertas en el contexto de exportación. Estos límites no convierten la IR en un verificador formal completo.

El proyecto todavía no soporta todo C sobre datos cuánticos ni afirma que programas C arbitrarios obtengan aceleración cuántica.

Ya existen ejemplos de Grover con predicado en otra función, QFT y estimación de una fase conocida: [functions.c](examples/functions.c), [fourier.c](examples/fourier.c) y [phase_estimation.c](examples/phase_estimation.c). Esto no completa todo C, una ABI cuántica general ni una demostración con presupuesto de error de todos los algoritmos. La [matriz de cobertura de C11](docs/c-coverage.md) distingue lo implementado de lo pendiente; el [plan de 1.0](docs/v1-plan.md) conserva los criterios adicionales de aceptación.

## 16. Pruebas realizadas y alcance de la evidencia

Las últimas ejecuciones completas registradas obtuvieron:

```text
Normal:                  353 passed, 0 failed
Cadena con ASan/UBSan:    353 passed, 0 failed
Frontend diferencial:    145 compilaciones aprobadas
Crecimiento sin tablas:   24 circuitos aprobados
QASM independiente:       11 circuitos aprobados
```

Incluyen:

- Compilación de programas reales mediante GCC y generación de ejecutables ELF.
- Separación entre datos clásicos y cuánticos, incluyendo metadatos de `int`, `quant int` y `quant bool`.
- Operadores, promociones, conversiones y operandos evaluados una sola vez.
- Preparación uniforme, entrelazamiento y mediciones repetidas.
- `if/else` coherente, controles anidados y limpieza de auxiliares.
- Grover con una y varias soluciones.
- Funciones directas, préstamos escalares y por punteros, productos enteros e inicialización desde CPU.
- Operaciones indexadas en bucles CPU, evaluación única y orden de medición/limpieza de operandos; rechazo de reinterpretaciones de punteros sin contrato.
- Actualizaciones indexadas con accesos equivalentes y aritmética modular que no introduce desbordamiento con signo al traducirlas.
- Fases parametrizadas, Fourier/inversa, control de cambios de base y estimación de fase.
- Actualizaciones inyectivas.
- Rechazos por lecturas implícitas, borrado, uso después de movimiento y efectos clásicos bajo control cuántico.
- Rechazo de incrementos/decrementos CPU, efectos en argumentos de procedimientos y ensamblador en línea bajo control cuántico coherente.
- Validación de expresión, controles y versiones antes de descomputar, tanto en exportación como en simulación.
- Rechazo de mutaciones, mediciones, resets y liberaciones de dependencias necesarias para una limpieza pendiente.
- Verificación independiente de tipos, aridades, campos duplicados, rangos conocidos y conversiones de salida de la IR.
- Rechazo de entradas falsas o modificadas de `find` y movimientos entre tipos C distintos; aceptación de alias `typedef` compatibles.
- Exportación y recursos.
- Rechazos explícitos por límites del simulador, IR malformada y liberación sucia.

Hay verificaciones de amplitudes e interferencia, además de pruebas de resultados medidos. Por ejemplo, se comprueba el cambio de signo del estado marcado, la distribución conjunta tras control coherente y la probabilidad de Grover.

Los registros de esas pasadas están en [build/final-normal.log](build/final-normal.log) y [build/final-sanitized-complete.log](build/final-sanitized-complete.log). Son artefactos locales de verificación, excluidos del control de versiones junto con el resto de `build/`.

La pasada `sanitize-all.sh` instrumenta pruebas, plugin, driver, backend, runtime, motor y ejecutables generados con ASan y UBSan. GCC anfitrión no está instrumentado. La detección de fugas se desactiva solamente en sus procesos porque GCC retiene reservas hasta salir; las comprobaciones de memoria y UB siguen activas en el plugin. Las comprobaciones escalares insertadas por GCC antes de la pasada se sustituyen por validación del dominio cuando operan sobre datos cuánticos; las del código CPU se conservan. El script anterior `sanitize.sh` sigue disponible para instrumentar el núcleo y las pruebas.

La suite incluye 21 expresiones comparadas contra C clásico sobre las 256 entradas de `uint8_t`; suma/resta y su inversa sobre 3720 pares de base de varias anchuras; ocho predicados sobre las 65.536 entradas de `uint16_t`; y QFT/inversa contra una matriz DFT independiente sobre las 126 bases de anchuras de uno a seis bits. Hay 1024 expresiones IR tipadas generadas con semilla fija, comprobadas sobre todas sus entradas e invertidas, y 4096 mutaciones de IR para ejercer parser y verificación. Las operaciones indexadas e inversas se prueban sobre todos los pares de base de dos registros de tres bits, además de fases relativas, evaluación única, préstamos, límites y rechazos de pérdidas de información. Esa exhaustividad cubre esos dominios finitos; no demuestra todos los programas C ni todas las anchuras posibles.

Los números anteriores describen esas ejecuciones registradas. Volver a ejecutar los scripts produce los resultados de la suite vigente en ese momento.

También se ejecutaron ejemplos de funciones, vectores, Fourier, estimación de fase e índices CPU con `-O0`, `-O2` y `-Os`: 15 ejecuciones correctas en esa comprobación. La entrada `1` por `scanf` del ejemplo de vector dinámico produjo `9`. Esas ejecuciones complementan los recorridos de dominios finitos de la suite.

Se comprobó que el demo ejecutara desde otra carpeta y que sus dependencias dinámicas no incluyeran Python ni un SDK cuántico.

Esto aporta evidencia de funcionamiento. No existe una demostración formal ni validación de equivalencia para todos los programas aceptados. Hay una campaña inicial reproducible del frontend; ampliarla sigue siendo una condición de 1.0.


La revisión añade pruebas sin tablas para los once operadores y sus inversas, operandos negativos, módulos controlados/inversos, retornos propios, uniones CPU y cachés. `scripts/fuzz_frontend.py` contrasta 29 programas (semilla 20261011) con referencias C a cinco niveles de optimización: 145 compilaciones. Encontró una secuencia introducida por GCC que ahora se conserva en la IR. La validación externa también detectó un complemento booleano de anchura incorrecta en el camino finito; se corrigió y tiene regresión. El estrechamiento temprano ambiguo de productos se rechaza conservadoramente; un producto de dos `uint16_t` puede calcularse primero en `uint32_t` con operandos convertidos a `unsigned`.

`scripts/validate_qasm.py` importa el QASM original con Qiskit y valida once circuitos: ocho mediante estados completos hasta fase global, con tolerancia `1e-10`, y tres mediante transcripciones de medición intermedia y reset. Usa 8192 medidas por circuito; `scripts/qasm_samples.c` recoge las transcripciones nativas y Aer ejecuta el circuito exportado. Incluye entrelazamiento, fases relativas, controles abiertos, Fourier, Grover y un módulo independiente. Los registros se guardan en `build/qasm-validation.log`, `build/frontend-fuzz.log` y `build/synthesis-growth.csv`. Las dependencias de validación se fijan en `scripts/requirements-validation.txt`; el runtime C no depende de ellas.

El workflow [.github/workflows/verify.yml](.github/workflows/verify.yml) reproduce estas verificaciones y archiva evidencia. Su ejecución remota todavía no se ha registrado en esta sesión. Las comparaciones externas no cubren todos los canales de reset ni el feedback CPU exportado.

## 17. Construcción y reproducción

La versión actual del driver es `0.4.0`; la versión 1.0 sigue siendo un objetivo sujeto a los criterios documentados. Se probó con GCC `11.4.0`.

Se necesitan Linux o WSL, GCC/G++, cabeceras de plugins correspondientes al GCC utilizado, binutils y libc/libm. El driver verifica que la versión de GCC coincida con la utilizada para construir el plugin.

Por ejemplo, en Ubuntu 22.04 con GCC 11:

```bash
sudo apt install gcc g++ gcc-11-plugin-dev
```

Si se utiliza otra versión de GCC, deben instalarse sus cabeceras de plugin correspondientes. El script también admite cabeceras locales extraídas y la variable `GCC_PLUGIN_INCLUDE`.

Los paquetes `.deb` descargados pueden eliminarse después de extraerlos. La carpeta `.deps` debe conservarse mientras la construcción utilice esas cabeceras locales; `build/` contiene los ejecutables y artefactos regenerables.

En Linux o dentro de WSL, desde la raíz del repositorio:

```bash
bash scripts/build.sh

build/cket examples/find.c --run --seed 7 -o build/demo

build/cket examples/find.c --emit-ast -o build/demo.ast
build/cket examples/find.c --emit-ir -o build/demo.qir.json
build/cket examples/find.c --emit-qasm -o build/demo.qasm
build/cket examples/find.c --resources -o build/demo.resources.txt

bash scripts/test.sh
bash scripts/sanitize.sh
bash scripts/sanitize-all.sh
```

También se puede ejecutar:

```bash
bash scripts/demo.sh

build/cket examples/prepare_measure.c --run -o build/prepare-demo
build/cket examples/coherent.c --emit-ir
build/cket examples/hybrid_loop.c --run -o build/loop-demo
build/cket examples/functions.c --run --seed 7 -o build/function-demo
build/cket examples/vector_function.c --run -o build/vector-demo
build/cket examples/fourier.c --run -o build/fourier-demo
build/cket examples/phase_estimation.c --run -o build/phase-demo
build/cket examples/dynamic_vector.c -o build/dynamic-vector-demo
printf '1\n' | build/dynamic-vector-demo
```

El último comando imprime `9`. El ejemplo rechaza entradas fuera del rango antes de asignar el vector cuántico.

`sanitize.sh` comprueba el núcleo y las pruebas; `sanitize-all.sh` verifica además el plugin, driver, backend, runtime y ejecutables generados. Este último construye su cadena en `build/sanitized/`, separada del compilador normal. Sus binarios y los demás archivos generados pueden retirarse después de terminar las pruebas; el código, los ejemplos, los scripts y las cabeceras necesarias para reconstruirlos se conservan.

Desde PowerShell en el entorno Windows utilizado para desarrollar el prototipo:

```powershell
wsl -d Ubuntu2204 --cd /mnt/e/Backup/GitHub/Quantum -- bash scripts/build.sh

wsl -d Ubuntu2204 --cd /mnt/e/Backup/GitHub/Quantum -- ./build/cket examples/find.c --run --seed 7 -o build/demo
```

Validación adicional reproducible (dependencias externas solo para las pruebas):

```bash
python3 -m venv build/validation-venv
build/validation-venv/bin/pip install -r scripts/requirements-validation.txt
python3 scripts/fuzz_frontend.py
python3 scripts/benchmark_synthesis.py
build/validation-venv/bin/python scripts/validate_qasm.py
build/cket examples/circuit_module.c --run -o build/circuit-module-demo
build/cket_backend --resources build/demo.qir.json --no-tables
```

El último comando falla si el predicado requiere una tabla; es una política explícita, no una autorización para cambiar el cálculo. El informe de crecimiento usa IR de registros con tablas desactivadas a todas las anchuras.

Opciones públicas del driver:

| Opción | Resultado |
|---|---|
| Sin acción de exportación | Compila y enlaza el ejecutable híbrido. |
| `--run` | Compila y ejecuta el binario generado. |
| `--seed N` | Fija la semilla utilizada al ejecutar mediante `--run`. |
| `--emit-ast` | Exporta el AST real de GCC. |
| `--emit-ir` / `--emit-quantum-ir` | Exporta la Quantum IR propia. |
| `--emit-qasm` | Exporta OpenQASM 3 para programas admitidos por el camino estático. |
| `--resources` | Informa recursos lógicos del circuito estático. |
| `--gcc compiler` | Selecciona el GCC utilizado. |
| `-o output` | Selecciona archivo de salida o nombre del ejecutable. |
| `--version` / `--help` | Versión y ayuda. |

Las opciones GCC adicionales se pasan como tokens individuales, por ejemplo `-I/ruta` o `-DNAME=1`.

Un binario generado se puede ejecutar directamente:

```bash
CKET_SEED=7 ./build/demo
```

La herramienta de backend admite:

```bash
build/cket_backend --validate build/demo.qir.json
build/cket_backend --emit-qasm build/demo.qir.json
build/cket_backend --resources build/demo.qir.json
build/cket_backend --state build/demo.qir.json
```

`--state` es inspección de depuración del simulador con las mediciones del documento omitidas, como se describe en la sección 12.

## 18. Especificación 1.0 y prioridades

El [contrato de 1.0](docs/v1-plan.md) fija el subconjunto y las condiciones de publicación. Los primeros bloqueos son aritmética estructurada y búsqueda sin conteo exhaustivo; sigue el análisis completo del flujo aceptado.

1. **Aritmética y complejidad:** extender los circuitos ya añadidos, cubrir signo y composición, conservar límites globales y publicar crecimiento de puertas/auxiliares.
2. **Búsqueda:** sostener contratos de cantidad conocida/desconocida, verificación de candidato y fallo; ampliar la campaña estadística sin contar soluciones.
3. **Flujo y propiedad:** completar el análisis de caminos, préstamos, retornos y temporales más allá de las uniones e invariantes conservadores actuales.
4. **Funciones:** completar la ABI de C y resultados generales sobre la composición y los módulos QIR existentes.
5. **GCC:** ampliar la evidencia sobre transformaciones previas, promociones, casts, cortocircuitos y efectos; `volatile` no constituye una prueba.
6. **Validación:** ampliar la comparación independiente, medidas intermedias, CI y fuzzing del frontend/IR.
7. **Ejecución:** enlazar planes estáticos/parametrizados ya compilados y evitar también la síntesis de la primera llamada.

QPU, memoria dinámica, todo C11 y bucles coherentes generales quedan fuera de esta primera versión. Los avances no equivalen a completar los siete contratos ni autorizan anunciar 1.0.

## Referencias técnicas

- [Documentación complementaria del lenguaje](docs/language.md).
- [API de plugins de GCC](https://gcc.gnu.org/onlinedocs/gccint/Plugin-API.html).
- [Controles y fase global en OpenQASM 3](https://openqasm.com/versions/3.0/language/gates.html).
- [Importación de OpenQASM 3 en Qiskit](https://qiskit.qotlabs.org/docs/api/qiskit/qasm3).
- [Amplitudes y estados en Qiskit](https://qiskit.qotlabs.org/docs/api/qiskit/qiskit.quantum_info.Statevector).
