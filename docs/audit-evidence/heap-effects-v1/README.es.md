# Escrituras coherentes y resultados de actualización del heap

Esta entrega añade escrituras mediante índice cuántico y resultados de asignaciones, incrementos y
decrementos sobre el [heap nativo](../../c11-heap.md). El resultado anterior de `p[i]++` se conserva
con su receta inversa; las asignaciones y el prefijo devuelven el valor convertido al tipo del
elemento. La ciberseguridad y el aislamiento de Windows/WSL permanecen sin cambios.

Se congelan **202 entradas de fuente** antes de reconstruir el compilador. La matriz normal usa
`-O0`, `-O2` y `-Os`; la instrumentada, `-O1`. Ambas recorren las dos políticas de síntesis. Estos
resultados tienen sus propias fuentes y no sustituyen los de las campañas históricas.

| Comprobación                                    | Resultado                                                                 |
| ----------------------------------------------- | ------------------------------------------------------------------------- |
| Estados complejos conjuntos completos del heap  | 174 puntos normales y 58 con ASan/UBSan.                                  |
| Programas válidos focalizados                   | 45 normales y 15 instrumentados.                                          |
| Diagnósticos de dominios o contextos pendientes | 51 normales y 17 instrumentados.                                          |
| Motor: aserciones mixtas de heap                | 16729 por configuración, incluidos seis esquemas nuevos de IR inválida.   |
| Regresión de propiedad y copia                  | 50 puntos completos y 28 inversas del flujo de puertas por configuración. |
| Núcleo nativo anterior                          | 395 comprobaciones normales.                                              |
| Runner 05                                       | Dos ejecutables nativos; exportación estática rechazada explícitamente.   |

Los números tienen alcances distintos. Un rechazo de un contexto C todavía pendiente no cuenta como
una implementación. La campaña focalizada no repite toda la suite amplia de historia ni la auditoría
completa del repositorio. [summary.json](summary.json) registra las fuentes, resultados y SHA-256 de
los archivos de evidencia.

El [validador](../../../scripts/validate-heap.py) observa el runtime enlazado real. Compara todo el
estado conjunto con fórmulas independientes y con otra interpretación del flujo de puertas, con
tolerancia `1e-10`, conservando las fases. Antes de cada punto se exige que no haya mediciones ni
reset. Las fórmulas incluyen datos actuales, valores anteriores, historia retenida, controles y
auxiliares en cero.

Los perfiles nuevos comprueban selección de escritura, asignación con resultado, valor anterior del
incremento, liberación mientras ese valor sigue vivo y ramas cuánticas activas/inactivas. Otro
perfil entrelaza previamente los datos con un control y usa un selector independiente: sus cuatro
amplitudes, fases y versiones retenidas se comprueban antes y después de deshacer el resultado y
liberar el buffer. Los perfiles anteriores de crecimiento, reducción y captura de índices CPU
también vuelven a ejecutarse.

El selector se calcula una vez, valida el índice completo antes de estrecharlo y recorre los
candidatos. Las cachés privadas se deshacen contra las versiones originales. Los datos desconocidos
retirados permanecen como salidas conjuntas; `free` no introduce una medición ni garantiza recuperar
todos sus recursos. El contrato no supone una QRAM de costo constante.

Los prototipos encontraron dos errores de integración: la validación estructural no creaba la receta
necesaria para deshacer un resultado y una rama inactiva perdía la dependencia de su control. Se
conservan sus fallos y las correcciones. También se evita copiar los marcadores internos al
restaurar promociones enteras. Los registros previos no se presentan como una instantánea completa
de fuentes que no se guardó.

ASan/UBSan instrumentan plugin, driver, motor, runtime, fixtures y observador; el validador
comprueba sus símbolos. GCC anfitrión permanece sin instrumentar y usa el wrapper existente con
atribución de fugas desactivada en sus procesos. Los ejecutables conservan detección de fugas. El
archivo `evidence.tar.gz` reúne fuentes, fixtures, comandos, salidas y códigos de retorno;
`earlier-attempts.tar.gz` conserva los prototipos.

Quedan pendientes resultados de actualización dentro de bucles CPU, pruebas de direcciones disjuntas
para operandos no secuenciados, adaptadores de bytes/cadenas sobre heaps y ABI o layouts generales.
El compilador distingue la falta de una prueba de direcciones de una afirmación de que todo ese C
sea indefinido. El perfil cotidiano, los flotantes cuánticos y la revisión final de los papers
siguen abiertos. No se demuestra ejecución en QPU física ni aceleración frente a CPU.
