# Integración completa del compilador de 242 entradas

El [CI 40510311815](https://github.com/igna-s/cket/actions/runs/40510311815) terminó aprobado el 27
de octubre de 2026 para `ac8f5bc64230a6319f199e661045b7078fa381b5`. Pasaron formato, los contratos
del formateador y la composición completa de `scripts/verify.sh`. Se verificaron los 242 blobs Git
contra los hashes de fuentes del
[SDK correspondiente](../../../output/sdk/cpu-returns/README.es.md).

Esta cápsula cierra la integración que figuraba pendiente en los informes anteriores de esas
fuentes. No modifica sus resultados históricos ni recalifica los cambios posteriores. En particular,
el SDK de 244 entradas tiene su propia campaña dirigida y su propio CI.

## Resultados y límites

La ejecución incluye compilación, validadores C, matrices de estados/fases/inversas y ejemplos, con
productores normales e instrumentados con ASan/UBSan. Los ejemplos registran 1509 comprobaciones por
configuración. El fuzzer completa 145 compilaciones por configuración, con semilla `20261011`; la
campaña de crecimiento contiene 24 circuitos por configuración y sus CSV coinciden.

QuEST contrasta 64 estados completos y seis casos de canales en cada configuración. Los estados se
comparan hasta fase global con tolerancia `1e-10`; los canales comparan distribuciones de 8192
transcripciones por backend. Las cinco comparaciones clásicas sobre 29 familias de cabeceras C11
prueban integración clásica. Los 2100 diagnósticos de libc son pruebas de frontera, no
implementaciones cuánticas de esas interfaces.

Se conservan las exclusiones de recursos y alcance de cada matriz: un CI completo aprobado no
demuestra todo C11, todos los estados ni vidas/alias/ABI generales. Tampoco acredita una QPU física,
aceleración o revisión científica completa. La candidata sigue siendo `1.0.0-rc.3`; la
ciberseguridad permanece clásica.

[summary.json](summary.json) contiene identidad, metadatos de GitHub, marcadores finales, resúmenes
de las diez matrices instrumentadas y hashes de los 58 archivos descargados. `evidence.tar.gz`
conserva los logs completos de GitHub, los artefactos, sus hashes, las 242 identidades de fuente y
los scripts de comprobación/archivo. Las fuentes están también en la
[cápsula de retornos CPU](../procedure-returns-v1/README.es.md).
