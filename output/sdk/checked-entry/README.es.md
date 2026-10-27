# SDK con entradas de función comprobadas

Esta candidata RC3 incorpora los préstamos compartidos de lectura y corrige las llamadas clásicas
que podían alcanzar un stub vacío de una función `quant`. Conserva los paquetes anteriores y sus
identidades. Usa WSL `Ubuntu2204` sin montar unidades Windows.

Desde la raíz del repositorio:

```cmd
output\sdk\checked-entry\run_sdk_demo.cmd
output\sdk\checked-entry\compile_and_run.cmd output\sdk\checked-entry\example_readonly.c
output\sdk\checked-entry\compile_and_run.cmd output\sdk\checked-entry\example_heap.c
```

El primer comando ejecuta 37 programas reconstruidos. Los siguientes compilan un archivo C autónomo
con el compilador incluido, en `-O2`, y lo ejecutan con el motor disperso y semilla 7. El lanzador
admite un archivo sin argumentos de programa; reemplaza su copia de trabajo en `user/program.c`.
Requiere Linux x86-64/glibc y GCC 11.4.0 (`gcc-11`) para compilar.

El paquete conserva 240 entradas de fuente, binarios, headers y los circuitos estáticos admitidos.
La extracción comprueba SHA256; `sdk-audit.json` identifica las fuentes y los flujos verificados.
Las funciones cuánticas se expanden mediante llamadas comprobadas. Sus punteros y su ABI clásica
general siguen pendientes: una entrada no comprobada se rechaza explícitamente, en vez de terminar
sin hacer la operación. Los callbacks CPU habituales siguen siendo clásicos.

El [contrato](../../../docs/c11-function-entries.md) y la
[evidencia](../../../docs/audit-evidence/function-entries-v1/README.es.md) delimitan las pruebas.
Quedan vidas/alias/ABI generales, flotantes cuánticos, perfiles de recursos y composición completa.
El modo denso tiene un límite de 24 qubits; el disperso y la representación llegan a 128 cables con
presupuestos de soporte y trabajo. No hay calificación de QPU física ni de aceleración. La
ciberseguridad permanece clásica.

El CI anterior se detuvo con `unmodelled retained version`. La
[corrección posterior de la referencia](../../../docs/audit-evidence/runtime-memory-reference-v1/README.es.md)
modela explícitamente una guarda privada cero: pasan 900 puntos de estado y 270 inversas completas.
Solo cambia el validador Python del checkout; los binarios y el archivo de este SDK conservan sus
identidades. El validador incluido en el archivo es el original. La integración completa sigue
pendiente.
