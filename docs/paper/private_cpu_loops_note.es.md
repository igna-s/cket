# Locales privados y bucles CPU desde llamadores coherentes

27 de octubre de 2026. Esta ampliación continúa la
[nota de locales inmutables](private_cpu_locals_note.es.md). Admite escrituras a enteros CPU
privados y bucles `for`, `while` y `do while` cuya condición es CPU pura, incluso desde un llamador
bajo control cuántico.

```c
static void rotate(quant const bool *x, unsigned count) {
    unsigned total = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (i == 2)
            return;
        total += i + 1;
        if (x[0])
            phase(.03 * total);
    }
}
```

Para cada entrada CPU, el controlador calcula un número clásico de iteraciones y sus parámetros. Las
puertas conservan el control cuántico del llamador. No se mide un qubit para decidir cuándo
terminar. Si el programa CPU termina, la secuencia produce una operación cuántica controlada; no se
prueba la terminación de todos los programas C ni se transforma una condición cuántica general en un
bucle CPU.

## Propiedad de los locales

Los locales admitidos son enteros automáticos, inicializados con expresiones CPU puras, sin
dirección tomada, almacenamiento estático, `volatile`, `_Atomic` ni `cleanup`. Cada escritura debe
ser una sentencia de asignación/incremento/decremento con operandos CPU puros. Las escrituras
permanecen en la misma rama cuántica donde se declaró el local. Una rama CPU pura puede seleccionar
sus actualizaciones; una condición cuántica adicional no puede modificarlos.

Así, un contador puede cambiar con la iteración CPU pero no almacenar el resultado de una decisión
sobre amplitudes desconocidas. Un local declarado dentro de una rama cuántica anidada puede
actualizarse dentro de esa misma rama y no exponer su dirección. Los argumentos CPU pasados por
valor que se modifican usan almacenamiento local; no escriben en el llamador.

Las condiciones CPU que contienen esta administración privada conservan su control nativo. Las
condiciones de bucle y las escrituras privadas se protegen con la consulta simbólica existente de
controles posibles. Un camino probado inactivo omite aritmética y cargas que no debe evaluar. Si el
control es posible, los operandos CPU deben estar definidos en C. Un `do while` conserva su
estructura, con operaciones cuánticas controladas y administración CPU protegida.

Los retornos CPU usan la bandera privada de salida y llegan a la limpieza léxica antes de continuar
el llamador. El análisis de propiedades y las versiones de historia cuántica se mantienen. Permitir
un contador CPU no autoriza clonar estados desconocidos, medir sin pedirlo ni borrar historia que
una inversa todavía necesita.

## Rechazo interno corregido y comprobación

Una elección CPU guardada podía compartir una expresión interna de consulta. Al reutilizarla en una
operación con historia, el compilador intentaba bajarla otra vez y rechazaba su propia consulta como
llamada CPU del usuario. Ahora identifica los nodos puros que él mismo construyó y conserva su
identidad. No autoriza funciones del usuario por coincidir en el nombre.

La [campaña dirigida](../audit-evidence/private-cpu-loops-v1/README.es.md) compara amplitudes
complejas con fórmulas CPU y con evolución independiente de puertas, exige auxiliares cero e
invierte la secuencia completa. Incluye fases, X, bucles anidados, entradas CPU 0/1/2/3/4 y retornos
antes del incremento. El ejemplo del SDK comprueba además un parámetro CPU modificado por valor y la
interferencia correspondiente a las iteraciones ejecutadas.

Quedan condiciones de bucle y salidas decididas por datos cuánticos, expresiones CPU con efectos
anidados, `break`/`continue` bajo control coherente y vidas/alias/ABI generales. También quedan
flotantes cuánticos e integración completa de estas fuentes. La candidata sigue siendo RC3; no se
afirma C11 completo, una 1.0 estable, QPU física o aceleración. La ciberseguridad permanece clásica.
