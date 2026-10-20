# Copia de cadenas hacia almacenamiento cuántico

Se añaden adaptadores parciales de `strcpy` y `strncpy`: destino entero cuántico comprobado, fuentes
cuánticas o CPU conocidas, NUL, cola preservada, padding, alias local del retorno y actualizaciones
con historia reversible. El [contrato](../../c11-string-copy.md) delimita el alcance. El inventario
pasa a **38 nombres de funciones parciales**, con 771 entradas pendientes, 85 parciales y 368 de
metadatos; ninguna se registra como completamente implementada.

La fotografía contiene **175 entradas de compilador**. El bloque 06 reúne cinco programas. El
ejemplo nuevo produjo en una ejecución nativa:

```text
source=97 copy=97,0,90 padded=97,0,0 word=195
```

La fuente está en superposición entre NUL y `a`. El propio programa compara los bytes medidos contra
libc. También comprueba una escritura parcial de 16 bits desde un objeto CPU y modifica ese objeto
después de la captura. Las muestras adicionales por semilla y la regresión nativa normal se
registran en `summary.json`; no son pruebas exhaustivas de estados o inversas.

La exportación estática usa los valores explícitos de `string_copy.bindings.json`; la plantilla sin
especializar se conserva como tal. El programa nativo captura el objeto CPU real y no recibe esos
valores desde la fixture.

La cápsula conserva tres intentos anteriores y sus 174 fuentes originales. El primero ejecutaba un
guard interno en CPU y terminaba con instrucción ilegal (132); el segundo llegaba a una síntesis
in-place inadecuada para una sobrescritura parcial (1). El tercero ya ejecutaba correctamente, pero
la exportación rechazaba los parámetros CPU sin especializar (1). La entrega corregida mantiene los
controles de dominio, retiene el destino anterior y especializa la exportación explícitamente.

Siguen aplazadas las matrices completas de amplitudes/fases, inversas, mutaciones, contextos GCC,
sanitizadores e integración amplia. Las campañas previas no califican las nuevas fuentes. No se
afirma ejecución en QPU física ni aceleración frente a CPU.
