# Break y continue CPU sin saltarse la limpieza cuántica

27 de octubre de 2026. Continúa la [nota de bucles CPU](private_cpu_loops_note.es.md). Los bucles
CPU del perfil cuántico admiten `break` y `continue` estructurados, incluso desde un llamador bajo
control coherente, cuando sus decisiones son CPU puras.

```c
static void visit(quant bool target, unsigned skip) {
    for (unsigned i = 0; i < 4; ++i) {
        if (i == skip)
            continue;
        if (i == 2)
            break;
        target ^= true;
        phase(.07 * (i + 1));
    }
    if (target)
        phase(.13);
}
```

Un salto nativo podría dejar atrás instrucciones de limpieza emitidas al final del bloque. La
transformación usa dos booleanos CPU privados por bucle: uno permite seguir iterando y otro permite
continuar el cuerpo actual. `continue` desactiva el segundo; `break` desactiva ambos. Las sentencias
posteriores quedan guardadas y el bloque llega a su limpieza.

El permiso del cuerpo se inicializa en cada iteración. La condición comprueba primero el permiso del
bucle. En `for`, el incremento depende de ese permiso: se ejecuta tras `continue` y se omite tras
`break`. En `while` y `do while`, `continue` llega a la condición; `break` evita volver a evaluarla.
Los retornos de procedimiento conservan su bandera independiente y siguen evitando el incremento
después de retornar. Cada bucle anidado tiene sus propios permisos.

Las decisiones CPU seleccionan una secuencia de puertas. Los controles cuánticos del llamador
permanecen sobre esas puertas; ninguna amplitud decide un booleano CPU. Si un salto depende de un
valor cuántico sin medir se rechaza. Un `continue` que atraviesa un `switch` requiere análisis
adicional de flujo y se rechaza en estos bucles. El `break` de un `switch` pertenece a ese `switch`,
no al bucle exterior. No se amplía el soporte general de `switch`.

El pasaje no transforma bucles puramente clásicos fuera de regiones cuánticas. Las condiciones y
efectos CPU ordinarios conservan sus reglas anteriores; dentro del control coherente siguen
exigiéndose predicados puros y escrituras privadas permitidas. Las condiciones, inicializadores y
actualizaciones siguen usando la protección simbólica existente de controles inactivos.

Los permisos son almacenamiento CPU, no qubits. No contienen información de una decisión cuántica ni
clonan estados desconocidos. Las versiones, préstamos y recetas de inversa mantienen sus contratos.
Esta transformación no permite asignar o liberar cualquier dueño cuántico dentro de un bucle: las
declaraciones cuánticas y las vidas generales conservan sus restricciones.

La [evidencia](../audit-evidence/loop-jumps-v1/README.es.md) distingue comparaciones con C nativo,
fórmulas de amplitudes complejas, evolución independiente de puertas e inversas completas. Incluye
incrementos y condiciones con efectos fuera de control coherente, saltos del bucle interior,
llamadas con retornos propios y ausencia de medición/reset antes de los puntos de estado.

Quedan saltos y condiciones de iteración cuánticos, `continue` a través de `switch`, flujo general,
efectos CPU anidados bajo control coherente, vidas/alias/ABI generales, flotantes cuánticos e
integración completa de estas fuentes. No se demuestra terminación para todo C, C11 completo, una
1.0 estable, QPU física o aceleración. La candidata sigue siendo RC3 y la ciberseguridad permanece
clásica.
