# Inversas condicionadas de préstamos: candidata 1.0.0-rc.1

La corrección conserva y transporta la receta inversa de un temporal cuando una escritura puede
activarse bajo control cuántico. Reconstruye el valor anterior desde su ubicación condicionada,
deshace el temporal y conserva el efecto de fase. Los contraejemplos originales con control cero y
control en superposición pasan de pureza 1 → 0.5 a 1 → 1, sin mediciones ni resets.

Se eliminan también copias privadas redundantes de decisiones reconstruibles desde raíces estables,
se preservan los contratos de épocas y ciclos y se corrige un certificado que trataba patrones
negativos como rangos no negativos. La síntesis booleana evita una palabra auxiliar innecesaria. La
[nota técnica](../../paper/conditional_inverse_note.es.md) explica las precondiciones.

**1368 puntos de estado complejo completo y 456 inversas completas:** 912/304 normales O0/O2 y
456/152 ASan/UBSan O1. Son 64 programas por nivel, con ambas políticas de síntesis. Incluyen bool,
enteros con/sin signo de 8/16/32 bits, asignación y copia/relleno de un byte, controles abiertos y
anidados, préstamos encadenados/movidos/aritméticos, función del mismo archivo y negativos -2/-1. Se
compara antes, durante y después del préstamo, con fases independientes y fórmulas explícitas,
dueños/cables estables y únicamente la historia anterior necesaria. No se califican por ello las
vidas generales, escrituras repetidas del frontend, destinos derivados o préstamos de 64 bits.

Las 34197 aserciones del motor comprueban estados e inversas de intercambios repetidos, máscaras
booleanas transportadas y rechazo de épocas/ciclos antes del intercambio. Ambas construcciones pasan
además las regresiones afectadas, con recuentos y ámbitos separados en `summary.json`. El bloque 05
normal ejecuta ocho programas y genera un QASM estático con recursos. La nueva demo se ejecuta otras
32 veces por construcción; se observan las cuatro parejas de control/fuente. La exportación QASM no
se presenta como una nueva comprobación independiente en otro SDK.

**Sigue abierto el contraejemplo de copia coherente no idéntica al heap:** pureza conjunta sectorial
1 → 0.5 y error de densidad 0.5 sin canales explícitos. Esta candidata no es la 1.0 estable ni
cierra el C cotidiano, la auditoría general o la actualización de los papers. La ciberseguridad
permanece clásica y sin cambios.

`evidence.tar.gz` conserva las 231 entradas exactas, construcciones terminales, herramientas,
inspección ASan/UBSan de siete componentes, comandos, estados y reproducciones.
`earlier-attempts.tar.gz` separa prototipos fallidos y sus fuentes: v1 retenía un control privado
redundante; v2 fallaba con negativos; un diagnóstico posterior localizó el límite de espacio del
caso de 32 bits. Esos intentos no se suman a los resultados finales. Los papers/PDF anteriores
conservan sus fuentes originales.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
build/validate_swap_rebind
python3 scripts/validate-rebound-borrows.py --output build/new-rebound-proof --levels O0 O2
bash scripts/run-block.sh 05
```
