# Locales CPU privados dentro de llamadas coherentes

27 de octubre de 2026. Ampliación posterior a la
[composición de retornos CPU](coherent_returns_note.es.md). Se permiten locales enteros automáticos,
inicializados con una expresión CPU pura, sin escrituras posteriores ni dirección tomada. Incluye
`bool`, enteros con y sin signo y declaraciones anidadas que ocultan un nombre.

```c
static void select_phase(quant const bool *x, unsigned n) {
    unsigned local = n + 1;
    bool done = n == 2;
    if (done)
        return;
    if (x[0])
        phase(.03 * local);
}
```

Para cada entrada clásica `n`, el local tiene un valor clásico determinado. Selecciona una secuencia
de puertas `U_n`. Con un control cuántico `c`, las puertas implementan
`|0><0|_c ⊗ I + |1><1|_c ⊗ U_n`; el local no recibe información de `c`. Una fase global de `U_n` se
vuelve relativa entre los brazos de `c` y debe conservarse. Los temporales y la historia cuántica
siguen sus contratos anteriores; esta ampliación no permite clonar un estado desconocido ni
descartar su historia.

El análisis inspecciona el cuerpo expandido, incluyendo inicializadores. Cualquier escritura o uso
de dirección revoca el permiso. Se excluyen almacenamiento estático, `volatile`, `_Atomic`,
destructores de `cleanup`, punteros y agregados. También se excluyen inicializadores que leen datos
cuánticos sin medir o realizan efectos CPU. Las declaraciones admitidas no son qubits.

Cuando los controles existentes se prueban inactivos mediante certificados simbólicos, se omite el
inicializador y se usa cero para ese local sin efecto en el circuito. La consulta no mira amplitudes
ni mide. Si el control es posible, el inicializador debe estar definido en C. Esto protege
divisiones por cero y cargas nulas de los caminos probados inactivos sin autorizar esos operandos en
caminos posibles.

La [evidencia dirigida](../audit-evidence/immutable-cpu-locals-v1/README.es.md) compara todas las
amplitudes con fórmulas explícitas y evolución independiente de puertas; exige auxiliares cero e
invierte la secuencia completa. Comprueba fases con signo, nombres ocultos, predicados booleanos,
retornos anidados y las fronteras de inicialización. Los resultados usan tolerancia numérica; no son
una prueba sobre reales exactos o todos los programas C.

Quedan pendientes locales mutables, bucles bajo control coherente, salidas decididas por datos
cuánticos y vidas/alias/ABI generales. El soporte de enteros CPU no implica flotantes cuánticos, C11
completo, QPU física o aceleración. La candidata sigue siendo `1.0.0-rc.3`; la ciberseguridad
permanece clásica.
