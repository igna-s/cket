# Heap cuántico nativo con tamaños de ejecución

Esta entrega añade almacenamiento dinámico para elementos enteros y booleanos: `malloc`, `calloc`,
`realloc` y `free`, capacidad CPU, alias locales y préstamos a funciones. El
[contrato de heap](../../c11-heap.md) declara los límites y los contextos pendientes. La
ciberseguridad permanece clásica y se conserva el aislamiento de Windows/WSL.

La campaña usa **201 archivos de entrada congelados**, reconstruidos en Linux sin montar unidades de
Windows. Los registros vinculan cada comando, su salida y su código de retorno. La configuración
normal usa `-O0`, `-O1`, `-O2`, `-O3` y `-Os`; la instrumentada, `-O1`.

| Comprobación                                                                | Resultado                                                                             |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| Estado complejo conjunto del heap, fases y limpieza de resultados derivados | 90 puntos normales y 18 con ASan/UBSan; ambas políticas de síntesis.                  |
| Programas válidos de límites, alias y vida                                  | 40 normales y 8 instrumentados.                                                       |
| Rechazos de dominios y propiedad                                            | 60 normales y 12 instrumentados, con diagnósticos semánticos concretos.               |
| Ejemplo dinámico con capacidades iniciales 2, 3 y 4                         | 15 ejecuciones normales y 3 instrumentadas.                                           |
| Runner 05                                                                   | Un ejecutable nativo; rechazo explícito de exportación estática.                      |
| Regresión de propiedad/copia                                                | 50 puntos completos y 28 inversas independientes por configuración.                   |
| Regresiones de motor/integración, por configuración                         | 16711 comprobaciones mixtas de heap, 395 nativas, 519 de bindings y 2332 de historia. |

Las cifras de las suites mezclan aserciones, programas y comparaciones; no se suman como estados
cuánticos independientes. Los resultados se conservan en [summary.json](summary.json).

La regresión de historia instrumentada se completó en dos segmentos. El controlador original agotó
su límite de 480 segundos después de completar los grupos hasta `observation_diagnostics`. Se
conservaron esos resultados y se reinició el grupo interrumpido junto con los restantes: 585
comprobaciones, mediante un controlador que incluye las mismas funciones del test congelado. El
agregado de 2332 corresponde a ambos segmentos; el trabajo parcial del grupo interrumpido no se
cuenta. No se presenta como una invocación original ininterrumpida. El registro del corte conserva
un estado de timeout y código de salida desconocido, sin contarlo como rechazo semántico.

El [validador del heap](../../../scripts/validate-heap.py) observa el runtime enlazado real y
compara las amplitudes completas con fórmulas de estados conjuntos y con una interpretación
independiente del flujo de puertas. Se comprueba el resultado antes y después de la inversa guardada
y de liberar el buffer. Antes de esos puntos no puede haber mediciones ni resets. Se incluyen el
estado del control, las versiones retenidas, los elementos actuales y auxiliares en cero; la
tolerancia es `1e-10`. No se comprueban únicamente frecuencias de medición.

Los perfiles cubren crecimiento con índice CPU capturado antes de cambiarlo, reducción que retira un
elemento todavía prestado por un resultado derivado y lectura con índice cuántico. `realloc`
conserva identidades; `free` retira el acceso público. Los estados desconocidos permanecen como
salidas conjuntas, mientras los conocidos se limpian reversiblemente al terminar sus préstamos. La
liberación no promete eliminar el costo de esa historia ni clonar estados desconocidos.

ASan/UBSan instrumentan plugin, driver, motor, runtime, programas generados y observador. El
validador verifica los símbolos de instrumentación. GCC anfitrión permanece sin instrumentar; el
wrapper existente desactiva la atribución de fugas en sus procesos y carga el plugin instrumentado.
Los ejecutables generados conservan detección de fugas y UBSan detiene los errores.

Los prototipos detectaron una dependencia dinámica rechazada demasiado pronto por el frontend y una
selección CPU que incluía registros ajenos: un índice fuera de rango terminaba en un diagnóstico de
presupuesto en vez del dominio esperado. La versión final resuelve solo el registro seleccionado por
el índice CPU capturado. También se ajustaron la segunda expansión de marcadores internos y las
consultas clásicas de identidad bajo control coherente. Los registros de esos intentos y de ajustes
de fixtures quedan en `earlier-attempts.tar.gz`; no se les atribuye una instantánea completa de
fuentes que no se conservó.

El chequeo auxiliar del inventario necesitó una continuación: la copia congelada había omitido
`docs/c11-library.json`, que no es una entrada compilada. Se conserva el `FileNotFoundError`
original y el comando repetido con ese fixture presente; no se cambiaron las 201 fuentes ni se
repitieron las pruebas ya aprobadas.

`evidence.tar.gz` contiene fuentes, fixtures, comandos, salidas, códigos de retorno y controlador de
reproducción. `summary.json` incluye SHA-256 de los archivos y fuentes. La comprobación de
integridad compara además las fuentes del commit real con las ejecutadas.

Permanecen pendientes escrituras del heap con índice cuántico, efectos con resultado, adaptadores de
bytes/cadenas sobre heaps, ABI y layouts generales, flotantes cuánticos y cierre integrado del
perfil cotidiano. Esta campaña tampoco reaudita los papers históricos, demuestra ejecución en QPU
física o demuestra aceleración frente a CPU.
