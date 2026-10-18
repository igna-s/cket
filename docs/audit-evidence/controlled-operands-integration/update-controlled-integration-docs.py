"""Update current docs while retaining dated source identities and failures."""
from pathlib import Path


def update(root):
    path = root / 'docs/estado-general-del-proyecto.es.md'
    text = path.read_text()
    start = text.index('**La división anticipada en condiciones anidadas')
    end = text.index('## Qué significa que una operación cuántica esté implementada', start)
    text = text[:start] + '''**Ya hay una corrección integrada de operandos enteros bajo control cuántico.** La
[integración de 138 entradas](audit-evidence/controlled-operands-integration/integration-summary.json)
conserva el dominio seleccionado de asignaciones, sumas, XOR, los absolutos enteros estándar y
`memset`. El XOR booleano y los bucles con operandos literales o capturas escalares puras conservan
sus actualizaciones reversibles sobre el mismo registro, evitando dependencias y registros extra.

La [regresión amplia](audit-evidence/controlled-operands-full/README.es.md) terminó correctamente
en normal y ASan/UBSan: 76 pasos cerrados, 665 compilaciones de fuzzing por configuración y 70
circuitos independientes con QuEST por configuración, 64 estados y 6 canales. Las reconstrucciones
separadas de integración pasaron, por configuración, 393 casos nativos, 1270 controles de operandos
y 40 ejecuciones de los cuatro programas exactos del [contrato](c11-controlled-operands.md).
La aplicación a Windows comprueba los hashes de las mismas 138 entradas; esa transferencia no
equivale a ejecutar la regresión completa en Windows.

Las [pruebas independientes de v5](audit-evidence/controlled-operands-v5/README.es.md) conservan
8000 estados complejos e inversas de operandos, más 1200 estados y 600 ejecuciones nativas de XOR
booleano y bucles. Se conserva el fallo de coordinación del primer controlador y la continuación
instrumentada, sin inventar un código de salida exitoso para el proceso original. Las dos regresiones
de v4 quedan como [evidencia histórica](audit-evidence/controlled-operands-regression-failure/README.es.md).

**Siguen existiendo bugs confirmados.** El registro anterior de 137 entradas conserva
[siete fallos](audit-evidence/coherent-arm-operands/failure-summary.json): seis perfiles enteros
tienen correcciones y pruebas positivas en la integración nueva; el ángulo de `phase` sigue
pendiente. La [matriz de cargas CPU](audit-evidence/controlled-operands-v5/pending-cpu-loads.json)
conserva siete perfiles, cinco niveles de optimización y ambos modos de builtins: 140 ejecutables
cuánticos aceptados fallan, mientras las 70 referencias C terminan bien. Incluye asignación, XOR,
suma, `memset`, `phase`, condiciones e índices. La integración actual sigue afectada por esa familia.

La corrección anterior de condiciones anidadas tuvo su propia integración de 137 entradas. Su
[regresión local completa](audit-evidence/coherent-control-full/README.es.md) y su
[CI remoto](audit-evidence/integrated137-remote-ci/README.es.md) también terminaron bien. Los 145
casos compilados de fuzzing por configuración del CI son distintos de las 665 compilaciones de la
campaña local ampliada. Los resultados se conservan con sus fuentes y alcances originales.

''' + text[end:]
    text = text.replace('En el árbol principal se reconstruyeron por separado las herramientas normales e\ninstrumentadas',
        'En su integración original se reconstruyeron por separado las herramientas normales e\ninstrumentadas')
    text = text.replace('El árbol principal coincide en todos esos hashes y tuvo reconstrucciones\nnormales e instrumentadas separadas:',
        'Su integración original coincidió en todos esos hashes y tuvo reconstrucciones\nnormales e instrumentadas separadas:')
    start = text.index('Los retornos locales comprobados de `memcpy`')
    end = text.index('## Qué falta implementar para el objetivo grande', start)
    text = text[:start] + '''Los retornos locales comprobados de `memcpy`, `memmove` y `memset`, las relaciones admitidas
entre direcciones y las distancias al mismo objeto siguen integrados. Los
[contratos](c11-memory-returns.md) describen sus límites. Las suites recurrentes pasaron 290 y 170
comprobaciones por configuración en la campaña completa de las 138 entradas actuales. Los fallos
anteriores y las campañas de 136 y 137 entradas conservan sus fuentes originales. Los consumidores
generales de punteros, sus tiempos de vida y la ABI general siguen incompletos.

''' + text[end:]
    start = text.index('La [integración actual]')
    end = text.index('Los [informes de auditoría de los papers]', start)
    text = text[:start] + '''La [integración actual](audit-evidence/controlled-operands-integration/integration-summary.json)
identifica las 138 entradas y sus pruebas separadas de instalación. La
[campaña completa de esas fuentes](audit-evidence/controlled-operands-full/summary.json) conserva
sus dos reconstrucciones, cada comando cerrado y todos los tramos de fuzzing. Las campañas de
134 y 137 entradas permanecen como resultados históricos; no se renombran como pruebas de las
fuentes nuevas. Los [informes originales de memoria](re-audit-2026-10-17-memory-expressions.md)
conservan sus propias reconstrucciones y sus interrupciones, con sus alcances declarados.

''' + text[end:]
    start = text.index('El próximo paso concreto es completar las pruebas amplias de v5')
    end = text.index('La [evidencia histórica de distancias]', start)
    text = text[:start] + '''El próximo paso concreto es corregir los ángulos de `phase` y las cargas CPU que todavía
se evalúan en ramas cuánticas inactivas. También siguen pendientes los consumidores generales de
punteros, tamaños, layouts, efectos y las demás familias de C11. **La integración nueva no
completa C11 cuántico ni el objetivo general.**

''' + text[end:]
    path.write_text(text)
    path = root / 'docs/c11-memory-returns.md'
    text = path.read_text()
    start = text.index('El adaptador parcial está integrado sobre 137 entradas.')
    end = text.index('## Qué representa el puntero', start)
    text = text[:start] + '''Los retornos y las condiciones se integraron originalmente sobre
[137 entradas](audit-evidence/coherent-memory-integration/integration-summary.json). Su
[regresión amplia](audit-evidence/coherent-control-full/summary.json) terminó correctamente.
La [integración posterior de 138 entradas](audit-evidence/controlled-operands-integration/integration-summary.json)
conserva estos contratos y añade la corrección de operandos enteros del cuerpo de las ramas.

''' + text[end:]
    start = text.index('Los [ocho contrastes posteriores]')
    text = text[:start] + '''Los [ocho contrastes posteriores](audit-evidence/coherent-arm-operands/failure-summary.json)
reprodujeron siete fallos en la versión anterior de 137 entradas. La integración posterior
corrige los seis perfiles enteros de asignación, suma, XOR, `memset` y absolutos; el ángulo de
`phase` y las cargas CPU inválidas siguen pendientes. El [contrato de operandos](c11-controlled-operands.md)
y el [informe nuevo](re-audit-2026-10-18-controlled-operands.md) separan los resultados actuales
de aquellos fallos históricos.
'''
    path.write_text(text)
    path = root / 'docs/repository-audit.md'
    text = path.read_text()
    start = text.index('La [integración del 18 de octubre]')
    end = text.index('La [integración de memoria del 17 de octubre]', start)
    text = text[:start] + '''La [integración de operandos del 18 de octubre](re-audit-2026-10-18-controlled-operands.md)
añade evaluación diferida de operandos enteros bajo control coherente, conservación del dominio
seleccionado e identificación de los absolutos estándar. Sus 138 entradas pasaron una campaña
completa normal/ASan/UBSan, 665 compilaciones de fuzzing y 70 circuitos QuEST por configuración.
La instalación separada pasó 393 casos nativos, 1270 controles de operandos y 40 ejecuciones de
los ejemplos exactos del contrato por configuración. Las pruebas de estados completas se
registran aparte: 8000 de operandos y 1200 de XOR booleano y bucles.

La integración anterior de retornos, distancias y condiciones conserva sus 137 entradas y su
[campaña amplia completada](audit-evidence/coherent-control-full/summary.json). El ángulo de
`phase` y las cargas CPU de ramas inactivas siguen siendo bugs conocidos. Las referencias C
correctas y los 140 fallos de la matriz de cargas se conservan; la biblioteca C11 sigue incompleta.
La ciberseguridad permanece clásica.

''' + text[end:]
    path.write_text(text)
    path = root / 'README.md'
    text = path.read_text()
    marker = '## Install and build\n'
    assert text.count(marker) == 1
    text = text.replace(marker, '''The [controlled-operand contract](docs/c11-controlled-operands.md) covers supported pure integer
operands in coherent assignments, XOR, addition, standard integer absolutes and byte fill. Its
[audit report](docs/re-audit-2026-10-18-controlled-operands.md) records complete normal/sanitizer
regression and separate installation checks. Inactive CPU loads and deferred `phase` angles still
have confirmed bugs; these limits and incomplete C11 coverage remain explicit.

''' + marker)
    marker = 'Validation includes 393 native checks;'
    assert text.count(marker) == 1
    text = text.replace(marker, '''The controlled-operand suite adds 1270 checks per configuration: 1000 defined native executions,
200 undefined-domain diagnostics, 60 CPU-effect diagnostics and 10 private-homonym executions.
Its 100 ordinary C references per configuration and independent phase/inverse proofs have separate
scopes. Diagnostic rejection does not implement the rejected C context.

''' + marker)
    path.write_text(text)
    path = root / 'docs/c-coverage.md'
    text = path.read_text()
    marker = '## Additional quantum transformations\n'
    assert text.count(marker) == 1
    text = text.replace(marker, '''The [controlled-operand contract](c11-controlled-operands.md) keeps supported pure CPU integer
calculations inside selected typed expressions for assignments, XOR, addition, integer absolutes
and byte fill. Reversible literal/scalar updates keep their in-place path. CPU loads through
invalid storage or invalid indices in inactive coherent arms still have confirmed bugs; general
conditional memory access is not implemented by this arithmetic correction.

''' + marker)
    marker = '`phase(theta)` accepts finite classical angles, constant or runtime-computed.'
    assert text.count(marker) == 1
    text = text.replace(marker, marker + '\nRuntime angle expressions can still execute CPU arithmetic from an inactive coherent arm; this\nis a confirmed pending bug, including division by zero that ordinary C would skip.')
    path.write_text(text)
    path = root / 'docs/audit-evidence/controlled-operands-v5/README.es.md'
    text = path.read_text()
    start = text.index('**V5 no está integrado.**')
    text = text[:start] + '''Esta cápsula conserva el estado de las pruebas específicas previo a integrar. Posteriormente,
la [regresión amplia](../controlled-operands-full/summary.json) terminó correctamente y la
[integración](../controlled-operands-integration/integration-summary.json) aplicó las mismas
138 entradas con verificaciones de instalación separadas. Permanecen abiertos `phase`, las cargas
CPU, la ABI general y C11 completo. La ciberseguridad permanece clásica.
'''
    path.write_text(text)
    (root / 'docs/c11-controlled-operands.md').write_text((root / 'build/controlled-integration-v1/contract-draft.md').read_text())
    (root / 'docs/re-audit-2026-10-18-controlled-operands.md').write_text('''# Auditoría del 18 de octubre: operandos bajo control cuántico

La corrección integrada conserva la evaluación de aritmética CPU pura en la expresión cuántica
seleccionada. Evita ejecutar divisiones de ramas inactivas, protege el dominio de XOR y llenado,
y conserva las definiciones necesarias para la inversa. Los absolutos enteros estándar se
reconocen por la declaración o builtin real. Los homónimos privados conservan su semántica C.

La revisión v4 añadía versiones a actualizaciones reversibles que no las necesitaban. Dos casos
de la suite nativa fallaban: XOR booleano y bucle estático controlado. V5 conserva el camino de
actualización sobre el mismo registro cuando no hay un cálculo CPU diferido que requiera una
prueba de dominio guardada. Los [fallos de v4](audit-evidence/controlled-operands-regression-failure/README.es.md)
permanecen disponibles con sus fuentes y registros.

## Evidencia de las fuentes actuales

La [campaña completa de 138 entradas](audit-evidence/controlled-operands-full/summary.json)
terminó con código 0: 76 comandos cerrados, reconstrucciones normales y ASan/UBSan, todas las
suites recurrentes, 665 compilaciones de fuzzing por configuración, 24 circuitos de crecimiento
y 70 circuitos independientes con QuEST por configuración, 64 estados y 6 canales explícitos.
El corpus de 133 índices se ejecutó en tramos, conservando generador, programas, referencia C,
cinco niveles, los 256 valores de byte y semilla 20261012. Es un agregado explícito de comandos,
sin atribuirlo a una sola invocación original de `scripts/verify.sh`.

La [integración separada](audit-evidence/controlled-operands-integration/integration-summary.json)
reconstruyó ambas configuraciones y pasó 393 casos nativos, 1270 controles de operandos y los
cuatro programas exactos del [contrato](c11-controlled-operands.md) en cinco niveles de
optimización y ambos modos de builtins: 40 ejecuciones por configuración. La aplicación a Windows
comprueba las mismas 138 entradas; no afirma repetir allí toda la campaña Linux.

La [cápsula de estados](audit-evidence/controlled-operands-v5/summary.json) conserva 8000 estados
complejos e inversas de operandos, con capturas CPU reales y simulación dispersa independiente,
además de 1200 estados y 600 ejecuciones nativas de XOR y bucles. El máximo de 34 cables de las
exportaciones de operandos no se presenta como ejecución densa nativa por encima de su límite de
24 qubits. El fallo de coordinación del primer controlador se conserva junto a la continuación
instrumentada; no se inventa un código de salida para el proceso original.

## Pendientes que las regresiones no eliminan

La evaluación diferida del ángulo de `phase` y las cargas CPU de ramas inactivas siguen fallando.
La [matriz de cargas](audit-evidence/controlled-operands-v5/pending-cpu-loads.json) conserva 140
ejecutables aceptados que fallan, frente a 70 referencias C correctas. Sus siete perfiles cubren
asignación, XOR, suma, `memset`, `phase`, condiciones e índices con aritmética inválida. El
compilador integrado sigue afectado por esos casos, aun con las regresiones anteriores exitosas.

El inventario C11 conserva 1224 entradas: 782 pendientes, 74 parciales y 368 de metadatos. Las
27 funciones parcialmente adaptadas no se consideran implementaciones completas. Quedan
flotantes, complejos, memoria dinámica, punteros y ABI generales y otras familias. No se afirma
ejecución en un procesador cuántico físico ni aceleración. La ciberseguridad permanece clásica.
''')
