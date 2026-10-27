# Integración completa de llamadas coherentes de 244 entradas

El [CI 40549661037](https://github.com/igna-s/cket/actions/runs/40549661037) terminó aprobado el 27
de octubre de 2026 para `cba9759429c710748dffea8a2cfb81649b613802`. Pasaron formato, contratos del
formateador y toda la composición de `scripts/verify.sh`. Se verificaron los 244 blobs Git contra
las fuentes del [SDK correspondiente](../../../output/sdk/coherent-returns/README.es.md).

La ejecución comprende validadores C y matrices de estados/fases/inversas, con productores normales
e instrumentados con ASan/UBSan. Los ejemplos registran 1509 comprobaciones por configuración. El
fuzzer completa 145 compilaciones por configuración, semilla `20261011`; se compilan 24 circuitos de
crecimiento por configuración y los CSV coinciden.

QuEST contrasta 64 estados completos hasta fase global, con tolerancia `1e-10`, y seis casos de
canales con 8192 transcripciones por backend, en cada configuración. La comparación estadística de
esos canales no prueba canales arbitrarios. Las cinco integraciones clásicas sobre 29 familias de
cabeceras y los 2100 diagnósticos de libc no implementan esas interfaces como operaciones sobre
datos cuánticos.

[summary.json](summary.json) conserva identidad, metadatos de GitHub, marcadores y resúmenes de las
once matrices instrumentadas, con hashes de los 59 archivos descargados. El archivo comprimido
guarda los logs completos, artefactos y comprobadores. Las fuentes se conservan también en la
[cápsula dirigida](../coherent-returns-v1/README.es.md).

Este resultado cierra el pendiente de integración de `cba9759`; no recalifica las revisiones
posteriores de locales, bucles y saltos, aunque tengan también 244 entradas. Las identidades son los
hashes, no la cantidad de archivos. El requisito ampliado del archivador local de 665 compilaciones
frontend pertenece a otra configuración; este CI usa el perfil predeterminado.

Se mantienen las exclusiones de recursos y de cada matriz. No se demuestra C11 completo,
estados/vidas/ABI arbitrarios, QPU física, aceleración o revisión general de los papers. La
candidata sigue siendo RC3 y la ciberseguridad permanece clásica.
