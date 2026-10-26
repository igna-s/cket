# Demo de bytes intactos de un buffer fijo

Candidata experimental `1.0.0-rc.3`. Desde la raíz del repositorio en Windows:

```cmd
output\demo\run_fixed_demo.cmd
```

Usa WSL `Ubuntu2204`, extrae por entrada estándar, verifica SHA256 y ejecuta longitudes 0/1 con
cuatro semillas. No recompila ni monta unidades Windows. Construida en Ubuntu 22.04, GCC 11.4.0,
Linux x86-64/glibc compatible. En Linux, extraer `cket-fixed-demo-linux-x86_64.tar.gz`:

```sh
sha256sum -c SHA256SUMS
CKET_SIMULATOR=sparse CKET_SEED=7 ./bin/fixed_coherence 1
```

Combina array, función, punteros prestados, control cuántico, longitud CPU y préstamo con fase.
Comprueba el valor medido contra su fórmula. Las ocho salidas reales están en
`demo-fixed-audit.json`. El paquete contiene ejecutable, fuente, licencia y hashes.

La [evidencia independiente](../../docs/audit-evidence/fixed-patches-v1/README.es.md) comprueba
coherencia, fases e inversas por separado. La
[nota técnica](../../docs/paper/fixed_patches_note.es.md) explica el byte intacto y la historia. La
[guía](../../docs/releases/v1.0.0-rc.3.es.md) enumera qué está hecho y qué falta. No certifica todo
C cotidiano ni una QPU física. Los PDF y demos anteriores conservan sus revisiones históricas. La
ciberseguridad permanece clásica y sin cambios.
