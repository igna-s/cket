# Retornos CPU dentro de una llamada coherente

27 de octubre de 2026. Nota sobre el frontend posterior a RC3. La evaluación finita conserva sus
fuentes y resultados en la
[cápsula de 244 entradas](../audit-evidence/coherent-returns-v1/README.es.md). Los manuscritos
generales mantienen sus revisiones históricas.

## Operación que se implementa

Para un valor CPU fijo `n`, un procedimiento que aplica una unitaria `U_n` puede llamarse bajo un
control cuántico. La operación sobre control y destino es `C(U_n) = |0><0| ⊗ I + |1><1| ⊗ U_n`. La
decisión CPU de retornar selecciona la secuencia de puertas; no observa el control ni el destino.
Esta fórmula se aplica a las familias unitarias de esta nota, no convierte toda escritura C en una
unitaria sobre el destino aislado. Las escrituras que pierden información conservan sus obligaciones
de historia y propiedad.

El [ejemplo](../../examples/coherent_returns.c) usa este procedimiento:

```c
static void transform(quant bool target, unsigned count) {
    if (!count)
        return;
    target ^= true;
    if (count == 2)
        return;
    target ^= true;
    phase(3.14159265358979323846);
}
```

| `count` | Unitaria del procedimiento           | Comprobación del programa                                                                                 |
| ------- | ------------------------------------ | --------------------------------------------------------------------------------------------------------- |
| 0       | Identidad.                           | El control termina en cero tras H.                                                                        |
| 2       | X.                                   | El llamador aplica otra X controlada para deshacer el entrelazamiento; el control termina en cero tras H. |
| 1 o 3   | Dos X y fase π, equivalentes a `-I`. | La fase global del procedimiento se vuelve relativa bajo control; H permite observar uno.                 |

El destino termina en cero en los cuatro casos. Los resultados observados se comparan con esa
fórmula, mientras que la validación de estados comprueba fases y auxiliares antes de medir. Una
salida numérica correcta por sí sola no probaría esta composición.

La tabla usa la fase π ideal; el código la representa en binary64 y las amplitudes se comparan con
tolerancia numérica, no mediante una igualdad simbólica de números reales.

## Bandera, predicados y no clonación

Cada llamada expandida conserva una bandera CPU privada. Los retornos la desactivan y los pasos
posteriores consultan su valor. Bajo un llamador coherente solo se permite este almacenamiento
introducido por el compilador; la regla no habilita escrituras CPU observables ni locales generales
del usuario dentro del control cuántico. Los predicados CPU puros que gobiernan la bandera siguen
siendo selección clásica; las operaciones sobre los datos mantienen los controles cuánticos.

Los argumentos y predicados no deben evaluar cargas o aritmética inválidas cuando los controles son
demostrablemente inactivos. La consulta existente usa valores conocidos, definiciones guardadas y
épocas de dependencia, con presupuestos de prueba. No mide ni consulta amplitudes. Si no demuestra
inactividad, considera posible el control y los operandos CPU deben tener significado definido. Los
efectos CPU visibles, los argumentos con efectos y los retornos decididos por datos cuánticos sin
medir siguen rechazados.

No se duplica un estado desconocido. La bandera describe flujo CPU independiente del estado; los
predicados auxiliares cuánticos se calculan y deshacen con las recetas existentes cuando sus
dependencias lo permiten. Las comparaciones de estado conjunto exigen que el espacio privado de
estas familias vuelva a cero.

## Evidencia y límites

Cinco familias combinan X, fases, ramas anidadas, llamadas anidadas y ángulos `double` clásicos.
Cada una usa cuatro etiquetas coherentes, cinco valores CPU, ambas políticas de síntesis y cinco
niveles GCC en normal. ASan/UBSan usa O1. Se comparan todas las amplitudes con fórmulas C y con una
evolución independiente de las puertas, y se invierte la secuencia completa. Las fronteras incluyen
argumentos y predicados inválidos en controles inactivos, cortocircuito y rechazos de efectos CPU.
Los `double` son parámetros clásicos de fase; no implementan almacenamiento flotante cuántico.

Son programas y estados finitos. Quedan bucles coherentes, salidas dependientes de datos cuánticos,
declaraciones/vidas generales bajo control, CFG, alias y ABI generales. Los perfiles dinámicos usan
ejecución nativa, no una exportación QASM plana. No se afirma una 1.0 estable, QPU física ni
aceleración. La ciberseguridad permanece clásica.
