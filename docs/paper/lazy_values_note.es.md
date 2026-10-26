# Valores anteriores en expresiones cuánticas con efectos

26 de octubre de 2026. Seguimiento del bloque de cadenas publicado en `6400aed`.

Esta revisión corrige lecturas anteriores que siguen vivas mientras se compilan incrementos,
decrementos y asignaciones seleccionadas. También actualiza referencias de historial que todavía
suponían que un identificador antiguo y el dueño actual eran siempre registros independientes.

## Fallos encontrados

Este programa definido en C era rechazado antes de ejecutarse:

```c
#include <quantum.h>
#include <stdio.h>

int main(void) {
    quant bool condition = false, value = false;
    quant bool previous = condition ? value++ : value--;
    unsigned old = measure(previous), next = measure(value), flag = measure(condition);
    printf("previous=%u value=%u condition=%u\n", old, next, flag);
    return old != 0 || next != 1 || flag != 0;
}
```

El resultado esperado es `previous=0 value=1 condition=0`: decrementar un `_Bool` falso convierte el
resultado entero negativo a verdadero. El valor de la expresión postfix sigue siendo el anterior,
cero.

Había tres problemas relacionados:

- Los operandos parcialmente serializados y el resultado de un brazo condicional permanecían en
  variables locales de C++, fuera de las cachés de lecturas guardadas. Una escritura posterior no
  transportaba esas referencias a su vista anterior. Podían conservar certificados de bits cero que
  ya no correspondían al registro actualizado.
- Una materialización demostrada inactiva conserva correctamente su inversa en el motor. El frontend
  podía perder ese préstamo y terminar un antecesor todavía necesario, mediante `qhistory_end`.
- En una expresión como `c ? c++ : c--`, el primer brazo inactivo podía conservar la inversa de la
  decisión. El siguiente brazo debía volver a materializarla antes de intercambiar su origen. La
  comprobación anterior solo consideraba definiciones que el frontend todavía marcaba como
  derivadas.

## Cómo se conservan los valores

Los operandos pendientes se registran ahora durante su vida local. El mismo transporte utilizado por
las lecturas guardadas actualiza su texto, dependencias y parámetros CPU capturados. Al terminar la
expresión, los parámetros vuelven al consumidor correcto; no se adjuntan a una reserva de
almacenamiento ni a una operación de retención de historia.

Después de una materialización condicional, los antecesores que pueden seguir prestados usan
disposición diferida. El runtime decide cuándo acaba su obligación real de inversa. La decisión se
vuelve a materializar cuando un brazo posterior puede necesitarla, incluso si una rama anterior
conservó la definición por estar inactiva.

Estas operaciones transforman recetas y puertas. No leen amplitudes para reconstruir entradas, no
miden una condición y no borran un estado desconocido para liberar espacio. La información
sobrescrita que no puede limpiarse sigue siendo historial explícito.

## Referencias independientes

El caso `if(c) x=false` conserva el dueño público de `x` y retiene el bit anterior solamente en la
rama activa. Su historial abstracto es `c && old_x`. El validador antiguo intentaba colocar
simultáneamente `old_x` y el nuevo `x` en el mismo identificador. También quedaban referencias
obsoletas en una rama cuántica dentro de un `if` CPU y en las expresiones seleccionadas.

La referencia nueva obtiene los valores de salida y fases de las fórmulas C. Las etiquetas
abstractas del historial proceden de las escrituras y decisiones de cada programa. En estos casos,
el historial termina en etiquetas de base: dos entradas conservan interferencia pública cuando sus
etiquetas históricas coinciden. Se exige esa misma relación en el circuito; una copia innecesaria de
un dato intacto que distinga entradas antes coherentes hace fallar la prueba.

Otro intérprete sigue las cuatro entradas originales por las puertas emitidas y compara el estado
complejo completo, incluidos los auxiliares en cero. Después invierte la secuencia unitaria por
separado. Las observaciones explícitas se cuentan y se desactivan para ese contraste; no se presenta
una medición como una operación reversible. El intérprete rechaza puertas distintas de las
preparaciones iniciales H y las operaciones X/fase admitidas por estos programas. Historiales en
superposiciones más generales necesitan otros productos internos y otros casos.

## Alcance y límites

El modo completo de `validate_history` conserva todos sus grupos. El modo `--coherent-reference`
concentra las comprobaciones de las referencias modificadas y permite instrumentarlas sin repetir
los grupos ajenos al cambio. Sus recuentos tienen ámbitos distintos: valores nativos, estados
completos, canales de historial e inversas no se suman como un porcentaje de C implementado.

La [cápsula reproducible](../audit-evidence/lazy-pending-v1/README.es.md) conserva 237 entradas
congeladas y los registros terminales. Pasaron las 2332 comprobaciones normales de historial y las
355 del modo dirigido bajo ASan/UBSan. Este último cubre 320 contextos de estado completo, incluidos
280 con referencias de canales e inversas independientes. No equivale a ejecutar la suite completa
con sanitizadores. También pasaron expresiones (3509) y núcleo (395) en ambas configuraciones, la
regresión de cadenas y los diez programas/exportaciones del bloque 06 en normal. Las catorce
exclusiones de cadenas por recursos permanecen registradas.

La [demo para Windows y Linux](../../output/demo/README-lazy.es.md) permite ejecutar los dos
programas sin recompilar. El lanzador verifica los archivos por SHA256 y usa el simulador nativo en
WSL aislado; las mediciones de la demo verifican valores, mientras que la coherencia se contrasta en
las pruebas independientes.

La corrección no completa las vidas, alias y ABI generales, todas las escrituras repetidas, los
perfiles grandes de enteros ni los flotantes cuánticos. La validación integrada y los papers
generales conservan sus pendientes. Los informes anteriores mantienen sus fuentes y fallos
originales. La ciberseguridad permanece clásica.
