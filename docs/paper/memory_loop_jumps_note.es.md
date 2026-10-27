# Memoria cuántica e historia después de break y continue CPU

27 de octubre de 2026. Audita la composición de los [saltos CPU](loop_jumps_note.es.md) con
`memcpy`, `memmove` y `memset` sobre vistas fijas del perfil. No requiere cambios al compilador,
runtime o motor publicados en `5f12080`; añade comprobaciones e integración de validación.

## Qué se contrasta

Cinco familias cubren copia sin solapamiento, movimiento en ambas direcciones, relleno y una
escritura parcial de palabra. Cada familia usa salida por `break`, salto por `continue`, ambos en un
`for`, y la combinación equivalente en `while` y `do while`. Los conteos de bytes dependen del
índice CPU original, incluyendo los índices omitidos. Las fases también dependen de ese índice;
ejecutar otra cantidad u orden de operaciones cambia las amplitudes esperadas.

Un bit `coin` prepara datos correlacionados y una fase relativa. Un préstamo `saved` guarda una
expresión del valor inicial y recibe una fase después del bucle. La referencia conserva los valores
viejos necesarios y compara todos los registros vivos antes, durante y después de las escrituras.
Las máscaras se deducen de los rangos de bytes de C y se contrastan con el IR trazado; no se acepta
la máscara del compilador como definición de la operación esperada.

Las vistas completas usan conteos 0/1/2. La familia parcial escribe 0/1 bytes desde el segundo byte
de una palabra de 16 bits, bajo el contrato little-endian de esta entrega Linux x86-64. El byte bajo
contiene un bit independiente preparado en superposición con fase. Copiar el byte alto debe
conservar ese bit bajo sin guardarlo también como historia de la escritura.

## Comprobación adicional de coherencia

La palabra parcial empieza en la superposición de `0xa506` y `0xa507`. En el subespacio del bit 0,
la fase inicial es `-0.23`; cada iteración ejecutada añade `0.05 * (i + 1)`. Su matriz de densidad
reducida debe conservar diagonales `1/2` y términos cruzados `exp(±iθ)/2`. Los otros bits se
contrastan en el estado conjunto completo. Esto comprueba ese subespacio finito, no todos los
estados posibles de un byte.

Una referencia de estado conjunto que conserva todos los cables viejos podría admitir una copia
redundante del bit intacto en historia. Por eso se contrasta también su densidad reducida pública.
Un mutante aplica conceptualmente CNOT hacia un auxiliar nuevo: conserva probabilidades de base y
entrelaza el auxiliar con el bit. La comparación debe rechazar la pérdida de términos cruzados. Esa
CNOT es una operación cuántica válida; el mutante viola el contrato de dejar intacta la coherencia
pública de ese bit. No se lo presenta como clonación independiente de un estado desconocido, que no
es lo que realiza una copia de base.

La [cápsula](../audit-evidence/memory-loop-jumps-v1/README.es.md) contiene estados complejos,
evolución independiente de puertas, máscaras, densidades e inversas completas. Los puntos de estado
se toman antes de cualquier medición/reset. El compilador y sus binarios no cambian; el SDK de
saltos conserva sus fuentes y hashes originales.

Quedan el llamador coherente de estas familias de memoria, estados arbitrarios, alias/vidas/ABI
generales, el resto de C11 y la revisión científica general. La integración completa de la campaña
nueva sigue pendiente. No se afirma una 1.0 estable, QPU física ni aceleración. La ciberseguridad
permanece clásica.
