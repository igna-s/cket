# Historial de retornos por máscara: referencia corregida

26 de octubre de 2026. La versión sigue siendo `1.0.0-rc.3`.

El validador anterior exigía identificadores separados para la palabra original, un intermedio y el
resultado. Después de una escritura por máscara, el dueño público puede conservar su identidad y la
historia guarda solo los bits sobrescritos. Una copia adicional del byte intacto alteraría su
coherencia; exigir esa disposición antigua era incorrecto.

Se actualizan dos archivos del validador dentro de 237 entradas congeladas. El compilador, motor y
runtime no cambian respecto de `8e65d2a`; se reutilizan sus binarios exactos y se reconstruyen los
validadores normales e instrumentados. Retornos pasa 290 comprobaciones y diferencias de punteros
pasa 170, en cada configuración normal y ASan/UBSan.

Los cien circuitos de acción de retornos tienen dos ámbitos:

- Cuarenta casos de palabras: veinte con entrada conocida y veinte que recorren los 65.536 valores
  de la palabra de 16 bits, repitiendo niveles de GCC, builtins y políticas de síntesis. Se
  comprueban el resultado C, fases, cables privados liberados en cero y clases de historial. Los
  valores con el mismo byte sobrescrito deben compartir etiqueta histórica; distintos bytes
  sobrescritos deben distinguirse.
- Sesenta casos booleanos conservan las referencias estrictas e inversas de escrituras y
  definiciones guardadas. Los cuarenta circuitos adicionales de distancias conservan ese mismo
  contrato de inversas guardadas.

En las palabras, otro intérprete recorre X/fase tras comprobar la preparación H canónica. Invierte
la secuencia completa, incluidas fases y preparación, combinando amplitudes y exigiendo el estado
cero inicial. Es una inversa independiente de puertas; no una inversa `qcompute` guardada del patch
materializado ni limpieza emitida por el compilador. El máximo de 49 cables usa acción etiquetada
completa, sin afirmar simulación densa de 49 qubits.

Un control adicional demuestra que la referencia no acepta cualquier circuito unitario: el caso
correcto pasa ambas políticas y tres mutaciones válidas de IR fallan con su diagnóstico exacto.
Cambian el valor público, una fase o añaden una historia redundante del byte intacto. Un error
genérico del proceso no cuenta como ese rechazo. Este control usa el harness normal; las suites
positivas completas también se ejecutan bajo ASan/UBSan.

`evidence.tar.gz` conserva fuentes, comandos, registros, mutaciones y hashes, además del fallo
anterior. Cierra este gate; el barrido restante y la integración completa tienen su propio alcance.
No certifica toda libc, C11, el perfil cotidiano completo, los papers generales ni ejecución física
en una QPU. La ciberseguridad permanece clásica.
