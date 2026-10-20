# Concatenación de cadenas cuánticas

Se añaden adaptadores parciales de `strcat` y `strncat`: longitudes originales capturadas antes de
escribir, destino entero cuántico comprobado, retorno local del destino, preservación del prefijo y
de la cola y actualizaciones con historia reversible. El [contrato](../../c11-string-concat.md)
incluye las restricciones de rangos, NUL, alias y vida de las capturas privadas.

La fotografía contiene **177 entradas de compilador**. Se construyó normalmente con GCC 11 y el
runner cerró con código 0: seis programas, seis circuitos y seis informes de recursos. La nueva
ejecución observó:

```text
leading=1 concat=97,98,0 bounded=120,121,0
```

La longitud inicial del destino está en superposición. El propio programa compara los bytes con libc
y también comprueba `strncat` desde una fuente sin NUL dentro del límite. Las muestras adicionales
por semilla y la regresión nativa normal quedan en `summary.json`; no certifican todos los estados,
fases o inversas.

El refactor comparte el ensamblado de bytes y palabras con `strcpy`/`strncpy`. El ejemplo anterior
sigue ejecutando y exportando con sus mismos bindings CPU explícitos. La cápsula guarda las fuentes
exactas, los logs, las muestras y las exportaciones.

Hay ahora **40 nombres de funciones parcialmente adaptadas**, con 769 entradas pendientes, 87
parciales y 368 de metadatos. Ninguna se cuenta como C11 completamente implementada. Las matrices
exhaustivas de coherencia/inversa, sanitizadores y contextos de compilación siguen aplazadas al
cierre. No se afirma ejecución en QPU física ni aceleración sobre CPU.
