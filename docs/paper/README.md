**Seguimiento actual, 26 de octubre de 2026:** la candidata sigue siendo RC3, con bloques
posteriores publicados. Las correcciones se describen en notas separadas de los manuscritos
históricos:

| Bloque                                     | Nota y evidencia                                                                                    | Alcance                                                                                                                              |
| ------------------------------------------ | --------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| Copias del heap, RC2                       | [Nota](heap_coherence_note.es.md), [233 entradas](../audit-evidence/heap-coherence-v1/README.es.md) | Corrige los contraejemplos publicados de copia coherente y bytes intactos del heap.                                                  |
| Buffers fijos, RC3                         | [Nota](fixed_patches_note.es.md), [236 entradas](../audit-evidence/fixed-patches-v1/README.es.md)   | Corrige el contraejemplo de escritura parcial de una palabra fija que quedaba abierto en RC2.                                        |
| Escrituras de cadenas                      | [Nota](string_writes_note.es.md), [237 entradas](../audit-evidence/string-writes-v1/README.es.md)   | Máscaras de los bytes realmente escritos, con catorce exclusiones por recursos.                                                      |
| Valores anteriores y efectos seleccionados | [Nota](lazy_values_note.es.md), [237 entradas](../audit-evidence/lazy-pending-v1/README.es.md)      | Corrige operandos pendientes y vidas de inversas. Pasan 2332 comprobaciones normales de historial y 355 dirigidas con sanitizadores. |

La [nota de préstamos de lectura](../c11-readonly-borrows.md) y su
[cápsula de 239 entradas](../audit-evidence/readonly-borrows-v1/README.es.md) corrigen otro rechazo
de composición y una escritura mediante cast. El SDK nuevo incluye ese plugin; las campañas de 237
entradas mantienen su alcance anterior. La nota distingue las referencias al mismo dueño de una
copia de estado y califica fases, auxiliares e inversas en programas finitos.

Estas campañas califican perfiles finitos de densidad, fases e inversas. La revisión general de
papers y la validación integrada siguen pendientes. Los PDF y resultados históricos conservan sus
fuentes originales; no se los recalifica automáticamente para el compilador actual.

El [mapa de afirmaciones y evidencia actual](current-evidence-map-2026-10-26.es.md) explica cómo
leer los manuscritos históricos junto con el SDK. La
[campaña C posterior](../audit-evidence/c-priority-integration-v1/README.es.md) completa 2332
comprobaciones de historial tanto normales como instrumentadas; no modifica el alcance histórico de
los 355 casos dirigidos de la tabla. Distingue también las comparaciones de exportación QuEST, la
semántica C, los diagnósticos de libc y los recursos lógicos frente al hardware físico.

La [nota de relleno completo](single_fill_note.es.md) y su
[evidencia](../audit-evidence/single-fill-v1/README.es.md) añaden una mejora de recursos: una
comparación usada como valor de `memset` baja de 26 a 18 cables y ya ejecuta en modo denso,
conservando la coherencia. La matriz finita mantiene explícitos dos límites de síntesis de 32 bits.

El [seguimiento de integración y ejemplos](../audit-evidence/integration-cli-paper-v1/README.es.md)
comprueba los tres bloques C exactos de ambos manuscritos con el compilador posterior a `be96caf`:
tres ejecuciones nativas y seis estados completos frente a sus fórmulas, con ambas políticas de
síntesis. También corrige la referencia de versión del test de ejemplos, que todavía exigía `0.4.0`.
Esta comprobación conserva los PDF y la evaluación histórica; no cierra la revisión científica
general ni el CI integrado.

> Revisión del 24 de octubre de 2026: se corrige el umbral de enumeración en ambos idiomas y se
> distinguen los límites del prototipo histórico de la prioridad actual de C cotidiano. La auditoría
> también comprueba la
> [evidencia finita de esa revisión](../audit-evidence/heap-transfer-v1/README.es.md), con su
> identidad de fuentes. Esto no certifica todo el compilador ni completa C11. Los
> [hallazgos y límites](re-audit-2026-10-24.md) explican el alcance de la revisión.

La [revisión cuántica posterior](../re-audit-2026-10-24-coherence.md) encontró pérdidas de
coherencia que los casos finitos publicados no detectaban. Conserva los fallos originales y sus
correcciones sucesivas; los cierres finitos actuales figuran en la tabla anterior. La
[nota de inversas condicionadas](conditional_inverse_note.es.md) y su
[evidencia de candidata](../audit-evidence/rebound-borrows-v1/README.es.md) corrigen además los
perfiles publicados de préstamos tras escrituras que pueden activarse. Las vidas y los alias
generales conservan pendientes. La
[corrección posterior de préstamos inactivos](../audit-evidence/inactive-borrows-v1/README.es.md)
preserva la limpieza con fase cuando se demuestra inactividad global. La demo de la nota no contiene
esas combinaciones. Su evidencia tiene el alcance de los programas y fuentes identificados en cada
campaña.

La entrega actual tiene una
[nota técnica de dos páginas](../../output/pdf/cket_memoria_runtime_nota_tecnica.pdf) y una
[demo ejecutable](../../output/demo/README.es.md) con funciones, arrays y `memcpy`, `memmove` y
`memset` de longitud CPU. La [fuente editable](runtime_memory_note.es.md) y la
[auditoría de la nota](evidence/runtime-memory-note-audit.json) vinculan el programa, las salidas y
la campaña de 221 entradas: 900 puntos de estado completo y 270 inversas completas.

La [ampliación de memoria](../c11-runtime-memory.md) tiene su propia validación. La revisión
publicada de los manuscritos históricos conserva las 210 entradas de su predecesor; integrar los
cambios nuevos en esa revisión sigue pendiente. Los resultados históricos permanecen fijados a su
revisión original.

Para regenerar solamente la nota de la entrega, con el entorno editorial instalado:

```bash
.deps/paper-venv/bin/python docs/paper/build_runtime_memory_note.py
```

El constructor comprueba texto y límites y renderiza las dos páginas para inspección. Su informe
reinicia la revisión visual a `pending`; solo debe marcarse aprobada después de mirar ambas páginas.

# Paper de C Ket

Manuscritos completos en español e inglés sobre `|c>` / cket 0.4.0 experimental. El estado estudiado
corresponde al commit `1ebb2e308e61bed5632798ded5beecf77b6b9a9c`.

El enlace de software de la referencia [1], `https://github.com/igna-s/cket`, respondió HTTP 404 sin
autenticación el 14 de octubre de 2026. La disponibilidad pública de ese enlace no quedó verificada;
la reproducción local utiliza el historial Git de este checkout. Los manuscritos conservan la
referencia histórica y esta limitación se registra fuera de sus resultados científicos.

Cada versión tiene **ocho páginas, incluidas figuras y bibliografía**. Su modelo de maquetación es
[Attention Is All You Need](https://arxiv.org/pdf/1706.03762): papel Letter, una columna de 5,5
pulgadas, cuerpo serif de 10 puntos, título centrado entre reglas, resumen sangrado, secciones
numeradas y tablas con líneas horizontales. Se usa Times New Roman embebida, compatible con la
Nimbus Roman del modelo; STIX es el fallback editorial. Las siete secciones conservan la progresión
del artículo de referencia y adaptan sus contenidos al compilador.

- Español: [PDF](../../output/pdf/c_ket_paper.pdf), [fuente editable](c_ket_paper.md).
- Inglés: [PDF](../../output/pdf/c_ket_paper_en.pdf), [fuente editable](c_ket_paper_en.md).
- Auditoría: [`audit.md`](audit.md).
- Figuras originales: [`figures/`](figures/), en PNG y SVG.
- Datos, logs y comprobaciones: [`evidence/`](evidence/).

Las siete figuras describen arquitectura, descomputación, Bell, Grover, memoria, puertas lógicas y
workspace. Cada caption identifica si presenta un esquema, un cálculo teórico o datos reproducidos
del backend. Las etiquetas, captions, tablas, texto y notas bibliográficas están traducidos. Los
títulos oficiales de los artículos citados y el código se conservan. Las figuras inglesas están en
[`figures/en/`](figures/en/); comparten datos y lógica de generación con las españolas. La auditoría
comprueba igualdad entre idiomas de bloques de código, ecuaciones, cifras y enlaces bibliográficos.
Las figuras son originales. El título, autoría y contenido corresponden a C Ket. Ambos manuscritos
utilizan redacción académica y omiten notas internas de edición, rutas del repositorio e
instrucciones de mantenimiento. La revisión exacta y los procedimientos de reproducción se
documentan aquí y en la auditoría; el cuerpo del artículo conserva métodos, resultados y
limitaciones científicas. La fuente anterior, más extensa, queda preservada en
[`evidence/manuscript-before-format.md`](evidence/manuscript-before-format.md).

## Repetir la validación

Usar Linux o WSL con GCC 11 y sus headers de plugin compatibles. Este comando reconstruye el
proyecto y renueva los logs de evidencia; la primera construcción de QuEST puede necesitar acceso a
Internet. Ejecutarlo en el commit indicado, preferentemente en un checkout separado. El checkout
actual incluye ejemplos posteriores a los 16 del estudio fijado, y sus resultados no deben
sobrescribir la evidencia de esta revisión del paper.

También se puede reproducir esa revisión desde el checkout actual sin cambiar los logs publicados.
El siguiente script crea un archivo separado del código histórico, reconstruye sus binarios y
verifica los recuentos y el CSV original:

```bash
bash scripts/reproduce-paper-baseline.sh
```

El snapshot y sus resultados quedan en `build/audit/paper-baseline/`. El script requiere que ese
destino no exista y no lo borra automáticamente. `audit_project.sh` y `capture_examples.sh` rechazan
una revisión distinta o cambios en el compilador; la campaña histórica exige GCC/G++ 11.4.0 y
registra los ejecutables seleccionados. Así se evita sobrescribir esa evidencia con la campaña
actual.

```bash
export CC=gcc-11
export CXX=g++-11
bash docs/paper/audit_project.sh
bash docs/paper/capture_examples.sh
```

## Regenerar los documentos

Las dependencias de Python son herramientas editoriales del paper. El compilador y sus pruebas
conservan su implementación en C/C++ y shell.

```bash
bash docs/paper/setup_pdf.sh
.deps/paper-venv/bin/python docs/paper/build_paper.py
.deps/paper-venv/bin/python docs/paper/audit_paper.py
```

`build_paper.py` genera ambos PDF y sus PNG/SVG de forma predeterminada. Las páginas renderizadas se
guardan en `tmp/pdfs/es/` y `tmp/pdfs/en/`. Se puede seleccionar una edición con `--language es` o
`--language en`; `--pdf-only` reutiliza las figuras ya generadas. Falla si una versión supera ocho
páginas; la bibliografía cuenta dentro de ese límite. El render utiliza PyMuPDF; el setup intenta
además disponer de un Poppler local. Las páginas deben revisarse visualmente después de cambios
editoriales. `audit_paper.py` no sustituye esa revisión: comprueba cifras, referencias, ejemplos,
resolución de figuras, texto y límites de página. También verifica tamaño Letter, columna, cuerpo
serif negro de 10 puntos, numeración y presencia de las diez referencias en la última página.
`audit_paper.py` revisa ambas versiones por defecto y admite el mismo selector `--language`. Sus
informes son `evidence/paper-audit.json` (ES) y `evidence/paper-audit-en.json` (EN); el manifiesto
SHA-256 incluye los dos PDF, las dos fuentes y todas las figuras. Ambas fuentes deben estar
presentes para comprobar la equivalencia editorial, incluso al auditar un solo idioma.
[`evidence/layout-reference.json`](evidence/layout-reference.json) registra las decisiones de
formato. Para reinspeccionar el modelo, descargar el PDF de arXiv en `tmp/pdfs/reference.pdf` y
ejecutar `inspect_reference.py` con el mismo entorno editorial; las capturas externas son
intermediarios de revisión.

El archivo `evidence/environment.txt` describe el entorno de pruebas C; `editorial-toolchain.json`
describe el entorno Python de publicación. El manifiesto SHA-256 vincula PDF, fuente, figuras,
datos, logs y archivos relevantes del proyecto. La auditoría detallada incluye su alcance y las
correcciones.
