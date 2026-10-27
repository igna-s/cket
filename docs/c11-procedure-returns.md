# Retornos CPU anticipados en procedimientos cuánticos

Los procedimientos `void` con parámetros cuánticos admiten `return;` dentro de bloques, condiciones
CPU y bucles CPU `for`, `while` y `do while`. Termina la llamada actual y continúa el llamador. Los
argumentos siguen sus contratos de préstamo; el retorno no transfiere ni copia su estado.

```c
static void visit(quant bool *value, unsigned limit, unsigned *visited) {
    for (unsigned i = 0; i < 4; ++i) {
        if (i == limit)
            return;
        value[0] ^= true;
        ++*visited;
    }
    value[0] ^= true;
}
```

El [ejemplo completo](../examples/procedure_returns.c) recibe un límite CPU y verifica visitas,
resultado e interferencia. Otro procedimiento calcula un temporal de lectura, aplica una fase y
retorna antes o después de una segunda fase. Su limpieza debe conservar la interferencia correcta.

```sh
CC=gcc-11 build/cket examples/procedure_returns.c -O2 -o build/procedure_returns
CKET_SIMULATOR=sparse CKET_SEED=7 build/procedure_returns 0
CKET_SIMULATOR=sparse CKET_SEED=7 build/procedure_returns 2
```

## Flujo y limpieza

La expansión introduce una bandera CPU local por llamada. Un retorno la desactiva. Las sentencias
posteriores se ejecutan únicamente mientras siga activa. Las condiciones de los bucles comprueban
primero esa bandera; el incremento de un `for` tampoco se ejecuta después del retorno. Así se
conservan el cortocircuito y los efectos CPU de C. Cada llamada anidada tiene su propia bandera, que
se inicializa de nuevo cuando se llama dentro de otro bucle.

El bloque llega a su limpieza léxica antes de continuar el llamador. Los temporales calculados se
deshacen mediante las recetas existentes cuando sus dependencias lo permiten. La historia
desconocida conserva sus obligaciones originales; retornar no autoriza borrarla, medirla ni crear
copias independientes. La bandera CPU no observa amplitudes ni se añade como un qubit del circuito.

La ampliación también descubre un fallo anterior, independiente de los retornos: ambas ramas de un
`if` CPU podían exportar certificados de un mismo valor conocido de entrada. La validación
estructural recorre las dos ramas del IR y rechazaba la segunda después de aplicar la primera. Las
expresiones dentro de esas ramas dejan de afirmar rangos o valores conocidos como hechos
incondicionales. El verificador conserva sus comprobaciones independientes y la ejecución nativa
sigue seleccionando una sola rama.

## Alcance y comprobación

La [evidencia](audit-evidence/procedure-returns-v1/README.es.md) distingue resultados C y efectos
CPU de estados complejos completos, fases, auxiliares limpios e inversas independientes. Incluye
llamadas anidadas y repetidas, condiciones con efectos, retorno antes del incremento, ramas anidadas
y la regresión de certificados anterior.

Los retornos de valor continúan con sus contratos anteriores: expresiones puras condicionales y un
resultado escalar propio terminal. Los retornos múltiples con efectos y resultado propio, salidas
condicionadas por datos cuánticos sin medir, `switch` y saltos generales requieren análisis
adicional. Los diagnósticos correspondientes se conservan. La transformación comprueba un límite de
128 niveles de anidamiento; las declaraciones, préstamos y vidas dentro del flujo siguen sus
restricciones existentes.

Las llamadas que necesitan la bandera de retorno todavía se rechazan dentro de un `if` cuántico:
falta componer ese flujo con la frontera de efectos CPU. Esto es una restricción de la
implementación, no una imposibilidad física de controlar una operación unitaria.

Esta entrega no cierra todos los flujos C ni toda la ABI, no demuestra ejecución en QPU física ni
aceleración. La ciberseguridad permanece clásica.
