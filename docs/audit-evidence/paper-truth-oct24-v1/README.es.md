# Revisión de veracidad de los papers

Revisión documental del **24 de octubre de 2026**. Se corrige una afirmación falsa del umbral de
enumeración en ambos idiomas y se separan expresamente los resultados históricos del núcleo
cotidiano actual. Los [hallazgos](../../paper/re-audit-2026-10-24.md) describen cada corrección, las
referencias consultadas y los límites de la revisión.

Ambas ediciones pasan 1122 comprobaciones automáticas por idioma, cuatro regresiones del umbral y
revisión visual de sus 16 páginas finales. Bell y Grover contrastan estados complejos completos con
fórmulas. La evidencia archivada del heap se vuelve a comprobar en sus 224 puntos y 56 inversas;
esto conserva el alcance finito de la entrega, sin repetir la campaña amplia del compilador.

`before.tar.gz` conserva los archivos anteriores desde el objeto Git `7d192b0`. `evidence.tar.gz`
conserva las fuentes documentales finales, PDF, estados, informes, manifiesto, comandos, resultados
y revisión visual. `summary.json` identifica sus hashes y los componentes usados en los ejemplos.
Los once artefactos científicos históricos permanecen idénticos; los scripts rechazan capturas desde
una revisión distinta antes de escribirlos.

El estudio histórico sigue fijado a `1ebb2e308e61bed5632798ded5beecf77b6b9a9c`; los ejemplos
actuales usan los componentes idénticos a la entrega de 210 entradas `heap-transfer-v1`. Las
comprobaciones editoriales, los estados y los casos nativos tienen unidades distintas. No se
certifica todo C11, el núcleo cotidiano completo, ausencia de bugs ni ejecución física.
