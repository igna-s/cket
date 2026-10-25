# Demo de la candidata 1.0.0-rc.1: limpieza e interferencia

Desde una consola Windows en la raíz del repositorio:

```cmd
output\demo\run_borrow_demo.cmd
```

Usa la distribución WSL `Ubuntu2204`, extrae el paquete en `/tmp`, verifica sus hashes y ejecuta
tres semillas sin recompilar ni montar unidades Windows. Necesita Linux x86-64/glibc compatible; se
construyó en Ubuntu 22.04 con GCC 11.4.0. El programa comprueba `value == expected` y devuelve cero
si coincide. El primer caso produce `control=0 source=0 value=1 expected=1`.

En Linux, extraer `cket-borrow-demo-linux-x86_64.tar.gz` y ejecutar:

```sh
sha256sum -c SHA256SUMS
CKET_SIMULATOR=sparse CKET_SEED=1 ./bin/borrow_cleanup
```

El paquete incluye el ejecutable, la fuente, licencia, hashes y `demo-borrow-audit.json`. El ejemplo
usa una función con puntero prestado, control en superposición, fase, limpieza de un temporal y
comprobación de interferencia. La [fuente del ejemplo](../../examples/blocks/05/borrow_cleanup.c) y
la [nota técnica](../../docs/paper/conditional_inverse_note.es.md) explican el programa.

La [evidencia de la candidata](../../docs/audit-evidence/rebound-borrows-v1/README.es.md) conserva
1368 puntos de estado completo y 456 inversas del bloque, con construcciones normales/ASan/UBSan. La
demo tiene pruebas adicionales en 32 combinaciones de semilla/política por construcción; el paquete
conserva tres ejecuciones de su binario exacto. Estos ámbitos no certifican todos los programas
posibles.

Es una candidata experimental. **Persiste un fallo en copias coherentes no idénticas al heap**, que
este ejemplo no utiliza. La [guía de versión](../../docs/releases/v1.0.0-rc.1.es.md) detalla lo
implementado y lo pendiente para la 1.0 estable. Los PDF previos conservan sus revisiones
originales. La ciberseguridad permanece clásica y sin cambios.
