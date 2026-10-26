# Demo de valores anteriores y efectos seleccionados

Desde la raíz del repositorio en Windows:

```cmd
output\demo\run_lazy_demo.cmd
```

Usa WSL `Ubuntu2204`, extrae por entrada estándar y comprueba SHA256. No recompila ni monta unidades
Windows. El paquete está construido para Linux x86-64/glibc compatible con Ubuntu 22.04 y GCC
11.4.0. En Linux, extraer `cket-lazy-demo-linux-x86_64.tar.gz` y ejecutar:

```sh
sha256sum -c SHA256SUMS
CKET_SIMULATOR=sparse CKET_SEED=7 ./bin/lazy_values_basis
CKET_SIMULATOR=sparse CKET_SEED=7 ./bin/lazy_values
```

Primero ejecuta el caso antes rechazado, con salida `previous=0 value=1 condition=0`. Después
ejecuta cuatro semillas de un programa con dos entradas en superposición, fases y un resultado
postfix seleccionado. El valor observado debe ser `condition ? 1 : !previous`. La
[fuente](lazy_values.c) y `demo-lazy-audit.json` conservan el programa y las ejecuciones normales e
instrumentadas. El paquete incluye fuentes, ejecutables, licencia y hashes.

Los resultados medidos se comprueban contra C. La coherencia, los canales de historial y las
inversas tienen [evidencia independiente](../../docs/audit-evidence/lazy-pending-v1/README.es.md)
separada. La [nota técnica](../../docs/paper/lazy_values_note.es.md) explica los fallos corregidos y
sus límites. La candidata sigue siendo RC3; la integración de todo el perfil cotidiano y la 1.0
estable continúan pendientes. La ciberseguridad permanece clásica.
