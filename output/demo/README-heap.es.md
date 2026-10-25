# Demo de escritura parcial coherente sobre heap

Candidata experimental `1.0.0-rc.2`. Desde Windows, en la raíz del repositorio:

```cmd
output\demo\run_heap_demo.cmd
```

El lanzador usa WSL `Ubuntu2204`, verifica los hashes y ejecuta cuatro semillas. No recompila ni
monta unidades Windows. Se construyó en Ubuntu 22.04 con GCC 11.4.0 y requiere Linux x86-64/glibc
compatible. En Linux, extraer `cket-heap-demo-linux-x86_64.tar.gz` y ejecutar:

```sh
sha256sum -c SHA256SUMS
CKET_SIMULATOR=sparse CKET_SEED=1 ./bin/heap_coherence
```

El programa usa una función con punteros prestados, un temporal con fase y una copia de un byte
hacia una palabra del heap bajo control cuántico. Conserva el byte alto y comprueba el valor medido.
El paquete incluye ejecutable, fuente, licencia, hashes y `demo-heap-audit.json`.

Salidas comprobadas con las semillas 1, 7, 13 y 29:

```text
control=1 high=0 value=0 expected=0
control=1 high=1 value=257 expected=257
control=0 high=1 value=256 expected=256
control=0 high=0 value=0 expected=0
```

Estas salidas por sí solas comprueban valores de base. La
[evidencia separada](../../docs/audit-evidence/heap-coherence-v1/README.es.md) compara densidades
públicas, estados conjuntos complejos e inversas con fases, antes y después de limpiar préstamos. La
[nota técnica](../../docs/paper/heap_coherence_note.es.md) explica el intercambio parcial.

**Hay un fallo abierto en escrituras parciales hacia palabras de buffers fijos.** La demo opera
sobre el heap. La [guía de versión](../../docs/releases/v1.0.0-rc.2.es.md) enumera lo corregido y lo
pendiente para la 1.0 estable. No hay ejecución demostrada en una QPU física. La ciberseguridad
permanece clásica y sin cambios.
