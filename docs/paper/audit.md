# Auditoría del paper de C Ket

**Resultado:** aprobado dentro del alcance descrito. Se entregan dos ediciones
completas, en español y en inglés. **Cada una tiene 8 páginas en total,
7 figuras originales, 4 tablas y 10 referencias**.
Revisión: 11 de octubre de 2026. El informe
[`paper-audit.json`](evidence/paper-audit.json) para español y
[`paper-audit-en.json`](evidence/paper-audit-en.json) para inglés registran
**175 comprobaciones automáticas aprobadas por idioma**. Se revisaron además
las 16 páginas renderizadas de las dos ediciones finales.

## Formato y estructura

El modelo solicitado es [Attention Is All You Need](https://arxiv.org/pdf/1706.03762).
Se descargó e inspeccionó su PDF v7: papel Letter de 612 x 792 puntos, una
columna de 396 puntos, cuerpo Times/Nimbus Roman de aproximadamente 10 puntos,
título centrado entre reglas, resumen sangrado, secciones numeradas y tablas
con reglas horizontales.

C Ket utiliza Times New Roman embebida de 10 puntos, títulos principales de
12 y título de 17,2; STIX sirve de alternativa si Times no está instalada.
El bloque de texto conserva 108 puntos de margen lateral. Una reserva de
2 puntos en párrafos compensa pequeñas diferencias de métricas al combinar
texto serif y código. Se comprobaron los límites geométricos de cada página,
los números centrados y los enlaces externos. Las referencias están completas
en la página 8: no hay anexos fuera del límite.

La progresión conserva siete secciones: introducción, antecedentes,
arquitectura, motivación y costos, configuración experimental, resultados y
conclusiones, seguidas de referencias. Los apartados específicos de modelos
de aprendizaje se adaptaron al estudio de un compilador. El título, autoría,
texto y figuras corresponden a C Ket; no se copiaron figuras, afiliaciones ni
marcas del artículo de referencia. Las decisiones están registradas en
[`layout-reference.json`](evidence/layout-reference.json).
La versión extensa anterior se conserva en
[`manuscript-before-format.md`](evidence/manuscript-before-format.md).

La versión inglesa conserva la misma estructura, cifras, contratos y límites.
Se traducen también diagramas, ejes, leyendas, captions, tablas y notas de
referencias; los títulos oficiales de los artículos se conservan. Los bloques
C, JSON y bash, las tres ecuaciones, las cifras del cuerpo y los diez enlaces
bibliográficos coinciden entre idiomas. Esta equivalencia se verifica de
forma automática además de la revisión editorial de la traducción.

## Alcance de la evidencia

Los resultados experimentales pertenecen al estado fijado
`1ebb2e308e61bed5632798ded5beecf77b6b9a9c`, cket 0.4.0 experimental:
GCC 11.4.0, Ubuntu 22.04 y WSL2 x86_64. Esta revisión de formato conserva
los logs del estudio anterior; no presenta sus campañas como nuevas ejecuciones.
Los ejemplos C y JSON exactos del manuscrito sí se volvieron a ejecutar
en el checkout actual.

| Evidencia del estudio fijado | Resultado | Archivo |
|---|---|---|
| Reconstrucción del proyecto | Aprobada | [build.log](evidence/build.log) |
| Suite nativa | 354 aprobadas, 0 fallidas | [native.log](evidence/native.log) |
| 16 ejemplos, cinco niveles GCC | 426 comprobaciones | [native.log](evidence/native.log) |
| Frontend diferencial | 145 compilaciones | [frontend.log](evidence/frontend.log) |
| Síntesis sin tablas | 24 circuitos | [growth.log](evidence/growth.log) |
| OpenQASM contra QuEST 3.7.0 | 11 circuitos | [qasm.log](evidence/qasm.log) |

El checkout se amplió posteriormente a 36 ejemplos y recibió una corrección
del compilador. Sus pruebas, incluidos sanitizers, son una campaña diferente
y no se mezclan con las 426 comprobaciones del estudio fijado. Para reproducir
esa tabla se debe usar la revisión indicada en un checkout separado.

Las cantidades de campañas distintas no se suman como cobertura homogénea.
Las 145 compilaciones cubren 29 casos en cinco niveles; las campañas internas
de 1024 expresiones y 4096 mutaciones son partes de casos agregados. En QuEST,
ocho circuitos comparan estados hasta fase global con tolerancia `1e-10`;
tres canales comparan 8192 transcripciones por circuito e implementación.
El importer independiente cubre el subconjunto OpenQASM emitido.

## Verificaciones de esta edición

Se comprobaron contra el código y los contratos locales el atributo `quant`,
el evento `PLUGIN_PRE_GENERICIZE`, la IR JSON propia, las dependencias de
descomputación, la ruta híbrida, la síntesis en la primera llamada y los
límites de 64 bits, 128 qubits representados y 24 simulados.

Los tres ejemplos C del manuscrito se compilaron y ejecutaron con semilla 7:
el programa completo imprime 12; el fragmento Bell da dos bits iguales;
la búsqueda imprime 3. Los fragmentos se envolvieron en un `main` e includes,
sin alterar sus operaciones. La instrucción JSON se validó y su estado
representa exactamente 7+5 en el destino. Estos checks de semilla no prueban
por sí solos la distribución probabilística: se contrastaron además los
estados almacenados de Bell y Grover.

Las figuras 1 y 2 son esquemas. Bell tiene probabilidad 1/2 en 00 y 11.
Grover usa N=16, M=1 y tres iteraciones; su probabilidad
`0.9613189697265625` coincide con el estado del backend, con normalización y
auxiliar limpio. La memoria de la figura 5 es teórica: 16 bytes por amplitud,
256 MiB a 24 qubits y 64 GiB a 32, excluyendo otras estructuras del proceso.

Las figuras 6 y 7 y cada celda de las tablas de puertas y workspace se
verificaron contra las 24 filas de [synthesis-growth.csv](evidence/synthesis-growth.csv).
Las preparaciones están incluidas; el resultado final permanece sin limpiar.
MCX y profundidad se mantienen como recursos lógicos, anteriores a
descomposición y mapeo físico. Los siete PNG superan 1500 píxeles de ancho
y cuentan con una versión SVG.

Las diez referencias se conservan citadas en el cuerpo y enlazadas en el PDF.
Sus fuentes primarias se verificaron durante la elaboración del estudio:
documentación oficial de GCC y OpenQASM, artículos/preprints de Bennett,
Grover, BBHT, Silq, Quipper, OpenQASM 3 y QuEST. La atribución de autor procede
de LICENSE; el enlace del repositorio privado es un identificador del proyecto,
no una descarga pública certificada.

## Revisión visual y trazabilidad

Ambos PDF finales se renderizaron con PyMuPDF a 144 dpi y se inspeccionaron
completos. Se verificaron en español e inglés:

- Página 1: título, reglas, autor, resumen, cuerpo e introducción.
- Página 2: arquitectura, fuentes del frontend y programa C completo.
- Página 3: ecuación, descomputación, circuito Bell, código y captions.
- Página 4: JSON, búsqueda, fórmula y curva de Grover.
- Página 5: memoria, límites y configuración experimental.
- Página 6: validación, curva de puertas y tablas de resultados.
- Página 7: workspace, profundidad y amenazas a la validez.
- Página 8: conclusiones, reproducibilidad y las diez referencias.

No quedaron páginas de desbordamiento, tablas partidas, captions separados
de sus imágenes, glifos ausentes ni texto cortado. Se corrigió la notación ket
de Bell para utilizar glifos matemáticos y se recalculó el espacio de etiquetas
de gráficos después de cambiar la tipografía. Se abreviaron las etiquetas de
escala logarítmica en ambos idiomas para evitar recortes del eje vertical.

El [manifiesto SHA-256](evidence/sha256-manifest.json) vincula ambos PDF finales,
fuentes, figuras en los dos idiomas, datos, logs, herramientas editoriales y archivos locales
relevantes. Los hashes de archivos de código corresponden al checkout actual;
los logs y el identificador de revisión describen el estado del estudio.
El manifiesto permite detectar cambios posteriores y no equivale a una firma
digital o auditoría externa.

La revisión acredita fidelidad documental dentro de este alcance. No es
revisión por pares, demostración formal ni cobertura exhaustiva del compilador.
No incorpora nuevas mediciones de tiempos, consumo físico, ruido o ventaja
computacional. Las conclusiones y los contratos probabilísticos mantienen
esas limitaciones.
