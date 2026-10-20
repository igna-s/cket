# Desplazamientos variables con menos auxiliares

El [selector reversible de posiciones](../../c11-shifts.md) sustituye los registros completos de
cada etapa por un resultado privado del ancho solicitado. Los circuitos completos de 32 bits que
antes excedían el límite físico ahora se generan sin tablas. El nuevo `variable_shift` del bloque 02
ejecuta en modo disperso con 105 qubits; el mismo código falla durante la compilación con el backend
anterior de 159 entradas, por el límite de 128 qubits. Ese fallo real se conserva.

Las 161 entradas congeladas pasan en normal y ASan/UBSan: 71 comprobaciones de desplazamientos, 3505
expresiones y 393 casos nativos por configuración. Las 16 nuevas comparaciones completas de estados
dispersos incluyen todos los bytes y ocho desplazamientos; dominios elegidos de 256 valores de 32/64
bits con todos los desplazamientos de esos anchos; ambas políticas de síntesis; fases de entradas y
resultado, controles coherentes, conservación de entradas y limpieza por inversa. No se presentan
como todos los valores posibles de 32/64 bits.

La construcción normal es nueva y completa. En ASan/UBSan se reconstruyen motor, runtime, driver,
backend y validadores utilizados; solo se reutiliza el plugin GCC sin cambios, comprobando sus
fuentes y hash. Los logs y mapas originales se conservan en `evidence.tar.gz`, junto con las
fuentes, la reproducción anterior y los circuitos/reportes del bloque. `summary.json` fija su
identidad.

El modo denso predeterminado sigue limitado a 24 qubits, el disperso conserva su presupuesto de
soporte y la representación de circuitos sigue limitada a 128. La generación completa de 64 bits con
entrada, contador y resultado independientes todavía excede esa representación. El bloque 02, C11
completo, la biblioteca completa y las campañas amplias de esta revisión siguen abiertos.
