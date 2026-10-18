# Operandos CPU en actualizaciones bajo control cuántico

La adaptación admite ciertos operandos enteros puros en asignaciones, XOR, sumas, las funciones
estándar `abs`, `labs`, `llabs`, `imaxabs` y `memset`. Su aritmética permanece en la expresión
cuántica tipada cuando evaluarla anticipadamente en CPU ejecutaría una rama inactiva. El dominio
seleccionado también forma parte de las definiciones guardadas para la inversa.

Las actualizaciones reversibles que usan literales o capturas escalares puras conservan su camino de
actualización sobre el registro existente. Conservar una versión adicional para cada una añadiría
dependencias innecesarias sobre el control y consumiría más registros en los bucles.

Estos contratos no completan C11. Las cargas CPU mediante punteros inválidos o índices cuya
evaluación es inválida en una rama inactiva siguen siendo bugs conocidos. La evaluación diferida del
ángulo de `phase` también sigue pendiente. Los efectos CPU bajo control coherente se rechazan con
diagnósticos explícitos; ese rechazo no cuenta como implementación de esos programas C.

## División en una actualización inactiva

Este programa conserva `x == 5` cuando la división no pertenece a una rama activa. No mide el
control para decidir si debe evaluar la división. Con una rama activa y divisor cero, el backend
debe informar que la división C está fuera de su dominio; no debe aceptar silenciosamente un
resultado numérico.

```c
#include <quantum.h>

int main(int argc, char **argv) {
    int k = argc > 1 ? argv[1][0] - '0' : 0;
    quant bool enabled = false;
    quant uint8_t x = 5;
    if (enabled)
        x ^= 10 / k;
    unsigned value = measure(x);
    unsigned flag = measure(enabled);
    return value != 5 || flag != 0;
}
```

## Valor absoluto dentro de una rama coherente

La llamada estándar se reconoce por su declaración o builtin. Una función del usuario llamada `abs`
mantiene su semántica C. El ejemplo usa un divisor válido y obtiene un resultado correlacionado con
el control. Las pruebas de estados completos comprueban la fase por separado; observar estas dos
salidas solamente no demuestra su preservación.

```c
#include <quantum.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int k = argc > 1 ? argv[1][0] - '0' : 5;
    quant bool enabled = superposition(0, 1);
    quant uint8_t x = 5;
    if (enabled)
        x = abs(-10 / k);
    if (enabled)
        phase(.48);
    unsigned value = measure(x);
    unsigned flag = measure(enabled);
    return value != (flag ? 2 : 5);
}
```

## XOR booleano y su inversa

El XOR booleano es reversible. Repetirlo restaura el valor original sin guardar una versión
adicional del registro actualizado. El control puede medirse después del bloque, preservando el
contrato de las dependencias del cálculo ya completado.

```c
#include <quantum.h>

int main(void) {
    quant bool enabled = superposition(0, 1);
    quant bool x = false;
    if (enabled)
        x ^= true;
    if (enabled)
        phase(.48);
    if (enabled)
        x ^= true;
    unsigned flag = measure(enabled);
    unsigned value = measure(x);
    return flag > 1 || value != 0;
}
```

## Bucle estático e inversa

El bucle siguiente suma seis en la rama activa y luego deshace esas sumas. El compilador conserva
las actualizaciones reversibles sobre el mismo registro. La prueba de integración verifica el
ejecutable con el límite nativo de 24 qubits; las pruebas independientes verifican también la fase,
los registros vivos y los temporales limpios.

```c
#include <quantum.h>

int main(void) {
    quant bool enabled = superposition(0, 1);
    quant uint8_t x = 250;
    for (int i = 0; i < 3; ++i)
        if (enabled)
            x += i + 1;
    if (enabled)
        phase(.48);
    for (int i = 2; i >= 0; --i)
        if (enabled)
            x -= i + 1;
    unsigned flag = measure(enabled);
    unsigned value = measure(x);
    return flag > 1 || value != 250;
}
```

Las fuentes, los resultados finitos y las limitaciones de la integración se publican junto a sus
hashes. Los circuitos se ejecutan en simuladores; esta ampliación no demuestra ejecución sobre un
procesador cuántico físico ni aceleración. La ciberseguridad permanece clásica.
