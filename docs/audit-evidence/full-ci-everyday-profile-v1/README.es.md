# Integración completa de línea base cotidiana de 247 entradas

El [CI 42507019584](https://github.com/igna-s/cket/actions/runs/42507019584) terminó aprobado el 8
de noviembre de 2026 para `5c5a965a1b24d629e5503abfacc25ec34978b142` con GCC `11.4.0`. Pasaron los
dos jobs del flujo heredado (`formatting` y `verify`), incluyendo verificación de formato, pruebas
del formateador y la ejecución íntegra de `scripts/verify.sh` en configuraciones normal y
ASan/UBSan. Se verificaron los 247 blobs Git en esa revisión exacta mediante `git cat-file --batch`.

La campaña cubre las 408 pruebas nativas en C y 1509 comprobaciones de ejemplos sobre 37 programas,
cinco niveles de optimización y contratos CLI/entrada en cada configuración. Ejecuta los 32
validadores C dedicados (además de `validate_qasm`) y las 12 matrices Python de estados, fases e
inversas completos (incluidos 55155 chequeos de parches de registros, 35871 de intercambios e
inversas y 16878 de montículo).

El fuzzer frontend completa 145 compilaciones por configuración con semilla `20261011`; se compilan
24 circuitos de crecimiento sin tablas por configuración con archivos `synthesis-growth.csv`
idénticos. QuEST `3.7.0` contrasta 70 circuitos QASM exportados (64 estados y 6 canales
estadísticos) por configuración. Las 5 integraciones clásicas sobre las 29 familias de cabeceras C11
y los 2100 diagnósticos de frontera sobre 210 interfaces de `libc` confirman rechazos y frontera
clásica sin reclamar implementación cuántica completa de `libc`.

[summary.json](summary.json) conserva los metadatos de GitHub Actions, los hashes SHA-256 de los 247
blobs fuente y de los 60 archivos de artefacto descargados, los 58 marcadores finales de registro y
los 12 resúmenes JSON de matrices sanitizadas. [evidence.tar.gz](evidence.tar.gz) archiva el
registro completo del runner, los manifiestos de verificación y copias exactas de los 60 artefactos.

Esta cápsula sella únicamente la línea base publicada en `5c5a965` previa al perfil cotidiano
ampliado de trabajo (1024 qubits físicos, división general, heap ampliado y aritmética IEEE-754 en
curso). Se mantienen las exclusiones finitas documentadas en cada matriz; no se prueba C11 completo,
ABI o vidas arbitrarias, QPU física ni aceleración cuántica, y la ciberseguridad permanece clásica.
