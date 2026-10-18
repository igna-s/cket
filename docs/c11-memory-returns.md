# Retornos de memoria y condiciones coherentes

Los retornos y las condiciones se integraron originalmente sobre
[137 entradas](audit-evidence/coherent-memory-integration/integration-summary.json). Su
[regresión amplia](audit-evidence/coherent-control-full/summary.json) terminó correctamente. La
[integración posterior de 138 entradas](audit-evidence/controlled-operands-integration/integration-summary.json)
conserva estos contratos y añade la corrección de operandos enteros del cuerpo de las ramas.

## Qué representa el puntero

En los casos admitidos, `memcpy`, `memmove` y `memset` devuelven la dirección de su destino. El
frontend conserva esa dirección como un alias comprobado en compilación. No coloca el valor de un
registro cuántico en un puntero CPU ni convierte la copia de valores de base en clonación de un
estado desconocido. Las escrituras conservan los contratos de historia e inversas de cada adaptador.

Se admiten inicializaciones locales de punteros compatibles, `void *` y vistas de bytes con
desplazamiento constante. El alias permite las lecturas, escrituras y llamadas de memoria que ya
admite el almacenamiento resuelto. Se conserva `const`; quitarlo no habilita una escritura. El
objeto y sus límites deben poder resolverse estáticamente.

```c
#include <quantum.h>
#include <string.h>

int main(void) {
    quant uint8_t source = 7, destination = 5;
    uint8_t *result = memcpy(&destination, &source, 1);
    unsigned observed = measure(*result);
    measure(source);
    return observed != 7 || result != &destination;
}
```

## Distancias y comparaciones

La resta de dos punteros admitidos al mismo objeto produce una distancia en elementos del tipo
apuntado. En una vista `unsigned char *`, produce una distancia en bytes. Se admiten signos
positivos y negativos y las direcciones situadas justo después del objeto cuando su uso cumple el
contrato. El resultado debe entrar en el tipo con signo correspondiente.

GCC representa primero la diferencia de direcciones en bytes y después aplica la división exacta por
el tamaño del elemento. El adaptador respeta esas dos etapas; no divide dos veces.

Las comparaciones dentro del mismo objeto se resuelven a partir de los desplazamientos. La igualdad
entre objetos se acepta solo cuando la identidad de su almacenamiento permite resolverla. El orden
entre objetos distintos, las direcciones potencialmente adyacentes y los casos sin información
suficiente no adquieren direcciones CPU inventadas.

```c
#include <quantum.h>
#include <stddef.h>
#include <string.h>

int main(void) {
    quant uint8_t bytes[2] = {3, 5};
    uint8_t *result = memcpy(bytes + 1, bytes, 1);
    ptrdiff_t forward = result - bytes;
    ptrdiff_t backward = bytes - result;
    unsigned first = measure(bytes[0]);
    unsigned second = measure(bytes[1]);
    return forward != 1 || backward != -1 || first != 3 || second != 3;
}
```

## Condiciones clásicas dentro de una rama cuántica

Una condición CPU pura puede aportar una entrada al predicado del circuito. Se captura su valor sin
medir el control cuántico exterior. La aritmética de una condición anidada permanece dentro de la
expresión condicionada, de modo que una división en una rama inactiva no se ejecuta antes en CPU.

```c
#include <quantum.h>

int main(int argc, char **argv) {
    int denominator = argc > 1 ? argv[1][0] - '0' : 0;
    quant bool enabled = 0;
    quant uint8_t value = 5;
    if (enabled) {
        if (value + 10 / denominator > 1)
            phase(.17);
    }
    unsigned observed = measure(value);
    unsigned control = measure(enabled);
    return observed != 5 || control;
}
```

Con denominador cero, este programa debe terminar correctamente porque la división está en una rama
inactiva. El fallo anterior aceptaba el programa y ejecutaba la división en CPU. El caso con
denominador cero y una rama alcanzable no se convierte en una operación definida.

Las lecturas puras de variables, campos, arrays e indirecciones CPU conservan el contrato de captura
existente: su almacenamiento debe ser válido al construir el circuito. La división condicionada no
resuelve los accesos CPU mediante una dirección inválida dentro de una rama cuántica inactiva. Ese
caso sigue pendiente. Los incrementos, escrituras y accesos observables CPU bajo control coherente
mantienen sus diagnósticos específicos.

## Qué se verificó y qué falta

El validador recurrente de retornos incluye 290 comprobaciones por configuración. El de distancias
incluye 170: 90 comprobaciones nativas, 40 diagnósticos y 40 circuitos de acción e inversa. Sus 45
ejecuciones de referencia en C ordinario se cuentan aparte.

La corrección de condiciones tiene evidencia separada de 180 ejecuciones nativas y 30 diagnósticos
por configuración, además de 800 comprobaciones independientes de estados complejos e inversas. Las
capturas de prefijos CPU y las ejecuciones del motor son pruebas distintas. Los circuitos de hasta
43 qubits se comprueban con referencias dispersas en sus dominios declarados; no se afirma una
simulación densa nativa de ese tamaño.

Quedan pendientes los consumidores generales de punteros devueltos, su escape o reasignación, los
tamaños y direcciones dinámicos, los tiempos de vida generales, el ABI indirecto y la biblioteca
cuántica enlazada por separado. Los rechazos de esos contextos no se cuentan como compatibilidad
implementada. Estas pruebas no completan C11 ni demuestran ejecución en hardware cuántico o
aceleración. La ciberseguridad permanece clásica y sin cambios.

Los [ocho contrastes posteriores](audit-evidence/coherent-arm-operands/failure-summary.json)
reprodujeron siete fallos en la versión anterior de 137 entradas. La integración posterior corrige
los seis perfiles enteros de asignación, suma, XOR, `memset` y absolutos; el ángulo de `phase` y las
cargas CPU inválidas siguen pendientes. El [contrato de operandos](c11-controlled-operands.md) y el
[informe nuevo](re-audit-2026-10-18-controlled-operands.md) separan los resultados actuales de
aquellos fallos históricos.
