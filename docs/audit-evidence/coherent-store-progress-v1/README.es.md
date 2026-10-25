# Progreso parcial de coherencia: buffers fijos

**Estado: progreso parcial con cuatro contraejemplos cuánticos todavía abiertos.** Las 222 entradas
congeladas identifican la corrección que preserva la rama inactiva de copias/asignaciones fijas
directas. Se conservan las vistas anteriores de expresiones GCC y las constantes entre brazos CPU.

La regresión nueva registra 240 puntos de estado complejo completo y 120 inversas: 160/80 normales
O0/O2 y 80/40 ASan/UBSan O1. Incluye cinco perfiles y ambas políticas de síntesis. Los estados de
memoria de ejecución y propiedad tienen sus propios recuentos dentro de `summary.json`; no se
agregan a los de esta regresión como una cifra de cobertura de C. Copia 414, relleno 940, núcleo 395
y operandos controlados 1270 pasan en ambas configuraciones, además de los contratos 15/23. Los
siete programas del bloque 05 pasan en ejecución nativa normal.

La evidencia nueva también reproduce cuatro fallos abiertos: limpieza de un préstamo no observado,
copia coherente del heap en el sector inactivo, autoasignación del heap y autorrelleno de un byte.
Cada uno reduce una pureza esperada 1 a 0.5 sin medir ni resetear. El movimiento del heap hacia la
misma dirección conserva la identidad en el caso contrastado. La calificación global queda abierta.

`evidence.tar.gz` contiene fuentes, construcciones, comandos, logs, herramientas de reproducción,
estados y contraejemplos separados bajo `known-open`. Los siete componentes instrumentados tienen
símbolos ASan y UBSan. Los scripts auxiliares conservan rutas originales de WSL; ajustar `root` y
usar directorios nuevos para repetirlos. La regresión mantenida se ejecuta desde el repo con:

```sh
python3 scripts/validate-coherent-overwrites.py --output build/recheck-coherent-store --levels O0 O2
```

`earlier-attempts.tar.gz` preserva los snapshots previos y logs de intentos incompletos. Los
previews mutables solo conservan los registros disponibles y tienen procedencia de compilador
incompleta; sus recuentos no se suman a los resultados finales. Dos referencias antiguas de memset
asumían historia incondicional: se reemplazan por la isometría condicionada, acción etiquetada
completa y reversión independiente del sufijo de puertas. Las inversas guardadas de efectos
ordinarios siguen comprobándose por separado.

El [informe](../../re-audit-2026-10-24-coherence.md) describe los límites. La nota técnica y su demo
conservan su revisión anterior de 221 entradas. Ciberseguridad permanece clásica y sin cambios.
