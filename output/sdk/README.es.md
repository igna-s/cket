**Paquete actualizado:** usá el [SDK con retornos CPU](cpu-returns/README.es.md). Incluye el plugin
corregido, 37 programas reconstruidos y el ejemplo de retorno, limpieza e interferencia. Este
directorio conserva el paquete anterior de 237 entradas con sus hashes y límites originales.

# C Ket listo para ejecutar y compilar

El paquete incluye el compilador, su plugin GCC, el runtime C, las fuentes y **37 programas
precompilados de los seis bloques**. Es la candidata experimental `1.0.0-rc.3` con las correcciones
publicadas; la versión estable y la integración completa siguen pendientes.

Desde la raíz del repositorio en Windows:

```cmd
output\sdk\run_sdk_demo.cmd
```

Usa la distribución WSL `Ubuntu2204`, comprueba SHA256 y ejecuta los 37 programas sin recompilar. La
transferencia usa entrada estándar; no monta unidades de Windows. Requiere Linux x86-64/glibc y GCC
11.4.0 (`gcc-11`) para compilar programas nuevos. El entorno WSL del proyecto ya tiene ese
compilador.

Para probar un archivo C propio:

```cmd
output\sdk\compile_and_run.cmd output\sdk\example_functions.c
output\sdk\compile_and_run.cmd output\sdk\example_heap.c
```

Puedes copiar y editar esos ejemplos y pasar la ruta del archivo modificado al mismo comando. El
primero usa funciones, arrays, tipos de 8/16 bits y recorridos; el segundo combina heap,
incrementos, bucles y `memcpy`. Ambos comprueban sus resultados medidos. Este lanzador recibe un
solo archivo C autónomo, sin argumentos para el programa. Compila en `-O2` y ejecuta con el motor
disperso y semilla 7. La copia de trabajo queda en `user/program.c` dentro del SDK en WSL y se
reemplaza en la siguiente compilación.

El lanzador muestra el directorio Linux de extracción. Desde ese directorio en WSL también puedes
usar la CLI completa:

```sh
CC=gcc-11 ./cket mi_programa.c -O2 -o mi_programa
CKET_SIMULATOR=sparse ./mi_programa
CC=gcc-11 ./cket mi_programa.c --emit-ir -o programa.json
```

El paquete conserva los circuitos estáticos y los informes que admite cada ejemplo. Los heaps y
bucles CPU dinámicos se ejecutan de forma nativa; no todos admiten exportación QASM estática.
`sdk-audit.json` identifica las fuentes, binarios y ejecuciones. La
[explicación del proyecto](../../docs/estado-general-del-proyecto.es.md) y el
[perfil cotidiano](../../docs/perfil-c-cotidiano.es.md) describen lo implementado y lo pendiente.

La ejecución usa un simulador cuántico de amplitudes complejas escrito en C. El modo denso tiene un
límite de 24 qubits; el disperso y la representación admiten hasta 128 cables con presupuestos de
soporte y trabajo. Funciones, punteros, memoria y biblioteca frecuente conservan sus contratos
parciales. Flotantes cuánticos, ABI/vidas/alias generales y varios contextos continúan pendientes.
Los ejemplos no certifican todo C11, todos los estados posibles ni ejecución física en una QPU. La
ciberseguridad permanece clásica.
