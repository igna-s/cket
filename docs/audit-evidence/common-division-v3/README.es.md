# Bloque 01 y división por constantes pequeñas

Se integra código utilizable: núcleo reversible de cociente por divisores de magnitud 2 a 16,
consumidores booleanos directos, ejemplo `quantum_buckets` y el comando `scripts/run-block.sh 01`.
El [contrato](../../c11-division.md#quotient-by-a-small-constant) describe la semántica y los
límites; la [guía de entregas](../../entregas-incrementales.es.md) permite ejecutar el paquete.

Las pruebas dirigidas del snapshot de 153 entradas terminan con salida cero en normal y ASan/UBSan:
3505 expresiones, 55 desplazamientos, 990 divisiones, 393 nativas y 271 actualizaciones por
configuración. Los 2310 nuevos dominios finitos recorren todos los bytes y etiquetas elegidas de
32/64 bits, ambas políticas de síntesis, controles, fases e inversas. Son recorridos de acción de
puertas; no constituyen simulación densa de todos los estados anchos ni una prueba nueva de estado
completo de todos los programas compilados.

Los núcleos provienen de las reconstrucciones de 152 entradas con producción idéntica. La revisión
de 153 entradas reconstruye el validador modificado. Se conservan los registros originales:
`tool_reuse.scope` contiene por herencia el texto incorrecto “preceding151”; la procedencia real es
`common-division-v2`, de 152 entradas, como muestran sus mapas y rutas. Esa corrección de lectura no
reescribe los registros cerrados.

La entrega añade el ejemplo y su runner, para 155 entradas. Su reconstrucción Linux nueva pasa los
ocho programas y exportaciones del bloque, y **1509 comprobaciones de 37 ejemplos** en cinco niveles
GCC. El reporte del ejemplo nuevo registra 16 qubits de pico. Esta construcción es distinta de la
reutilización de núcleos de la calificación anterior.

La comparación de recursos ya cerrada conserva 32 rechazos del frontend en las fuentes anteriores de
151 entradas, frente a 32 compilaciones y 64 generaciones de circuitos aprobadas en las 152 entradas
con producción idéntica al código integrado. Son ocho perfiles finitos, dos niveles GCC, dos modos
de builtins y, cuando la compilación permite llegar al backend, dos políticas de síntesis. No
equivale a ejecutar esos registros anchos en el simulador nativo. También se conserva el primer
intento fallido del controlador de referencia, que suponía que exportar IR no podía agotar recursos.

`integration-summary.json` identifica los mapas de fuentes, alcances y hashes de los artefactos.
`evidence.tar.gz` conserva fuentes, registros, logs y los circuitos/reportes pequeños del bloque. No
se declara aprobada una regresión amplia ni el CI de estas entradas nuevas. La campaña de 151
entradas sigue siendo una revisión distinta. C11 completo, la biblioteca completa y ejecución en
hardware cuántico siguen abiertos. La ciberseguridad permanece clásica.
