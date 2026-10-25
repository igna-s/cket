# Demo funcional de buffers y longitudes

Desde una consola Windows, en la raíz del repositorio:

```cmd
output\demo\run_memory_demo.cmd
```

Ejecuta los casos de longitud 0, 1 y 2 con WSL, sin recompilar ni montar unidades Windows. Cada caso
comprueba sus resultados y devuelve 0 si coinciden. El binario usa el simulador disperso con
`CKET_SEED=7`; distintas operaciones pueden producir muestras distintas.

En Linux x86-64, extraer `cket-memory-demo-linux-x86_64.tar.gz` y ejecutar:

```sh
CKET_SIMULATOR=sparse CKET_SEED=7 ./bin/buffer_lengths 2
```

El paquete incluye fuente C, licencia, `SHA256SUMS` y resumen de evidencia. Se construyó en Ubuntu
22.04 con GCC 11.4.0; ejecutar requiere Linux/glibc compatible, sin GCC ni SDK externo.
`demo-audit.json` identifica el código y registra las ejecuciones comprobadas.

Salidas comprobadas desde el lanzador Windows; cada caso terminó con código 0:

```text
length=0 output={7,11} source=3
length=1 output={1,11} source=9
length=2 output={2,3} source=3
```

La [nota técnica](../pdf/cket_memoria_runtime_nota_tecnica.pdf) contiene el código completo, el
modelo reversible y los resultados de la campaña de estados e inversas.

Código de la entrega: `da2ffb31e568026d58b8e5d98aa31929171cfac0`. El
[contrato](../../docs/c11-runtime-memory.md) explica los límites. El C cotidiano completo sigue en
desarrollo y ciberseguridad continúa clásica.
