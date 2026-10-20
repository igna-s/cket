# Ejecución dispersa y avance de enteros anchos

El [nuevo modo](../../simulacion-dispersa.es.md) ejecuta estados de soporte ocupado acotado sobre
hasta 128 qubits físicos. Los dos ejemplos del bloque 02 usan realmente `int` de 32 bits y `size_t`
de 64 bits en el runtime C; sus circuitos requieren 99 y 84 qubits respectivamente. Este avance
utilizable mantiene abierto el bloque 02 y el objetivo C11 completo.

Las 159 entradas congeladas pasan por configuración normal y ASan/UBSan: 88 comprobaciones nuevas,
393 pruebas nativas y 1509 comprobaciones de los 37 ejemplos anteriores. Las nuevas pruebas comparan
amplitudes complejas en 32 dominios de puertas/fases e inversas, canales explícitos, controles
abiertos/cerrados, cables altos hasta 127, límites de soporte y ambos programas nuevos en
`-O0`/`-O2` con ocho semillas. Son dominios finitos, no todos los circuitos de 128 qubits.

La construcción normal es nueva y completa. En ASan/UBSan se reconstruyen todos los componentes del
motor, runtime, driver, backend y validadores utilizados. Únicamente se reutiliza el plugin GCC
instrumentado anterior, comprobando sus fuentes y hash. No se presenta como reconstrucción completa
nueva del plugin.

Se confirma un bug real previo de medición: una probabilidad próxima a `1e-16` quedaba descartada
por el umbral denso de `1e-15`. Con la misma preparación y semilla, la biblioteca anterior produce 0
y la nueva produce el 1 esperado por el muestreador. El código de salida 1 de la reproducción
anterior se conserva como fallo confirmado, separado de las pruebas actuales aprobadas.

`summary.json` identifica fuentes, alcance y hashes; `evidence.tar.gz` conserva las fuentes,
comandos, salidas, reproducción anterior y exportaciones pequeñas de los dos ejemplos. La campaña
amplia anterior de 151 entradas no certifica esta revisión; la regresión amplia y CI de estas 159
entradas siguen pendientes. El modo denso predeterminado conserva su límite de 24 qubits. El modo
disperso falla explícitamente si supera su presupuesto, conserva las limitaciones numéricas de
`double complex` y no implica ejecución en QPU ni aceleración demostrada.
