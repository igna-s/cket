# Posiciones y direcciones de búsquedas

Se amplían los consumidores de `memchr`, `strchr`, `strrchr`, `strpbrk` y `strstr`: resta de la
posición encontrada y comparación con una dirección comprobada del mismo objeto. La primera y la
última coincidencia son distintas; una búsqueda fallida no produce un offset válido. Continúan
pendientes los punteros devueltos generales, su almacenamiento y su desreferencia.

La fotografía tiene **170 entradas de compilador**. Una reconstrucción normal y el runner del bloque
06 terminaron con código 0. Se ejecutaron tres programas y se exportaron tres circuitos e informes
de recursos. El ejemplo nuevo produjo:

```text
text=aa first=0 last=1 raw=0 member=0 substring=0 double=1 b=0
```

Son muestras nativas con referencia libc en el propio programa; no certifican todo el estado
cuántico. `summary.json` conserva además las observaciones adicionales por semilla, sin presentarlas
como una matriz exhaustiva. Las fases, inversas, capturas CPU, contextos de compilación,
sanitizadores e integración general siguen pendientes de calificación al cierre.

`evidence.tar.gz` conserva las fuentes, logs y exportaciones exactas. También guarda la compilación
fallida anterior y sus 170 fuentes: el refactor dejó una referencia a `location` fuera de alcance.
El intento corregido permanece separado del fallo. Esta entrega amplía las mismas cinco interfaces
parciales; no aumenta el contador de nombres de funciones ni completa C11.

El [contrato](../../c11-string-searches.md) describe las precondiciones y limitaciones de los
consumidores de posiciones. La ciberseguridad permanece fuera de esta adaptación.
