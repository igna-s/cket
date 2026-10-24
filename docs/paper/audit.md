# Auditoría del paper de C Ket

**Resultado:** aprobado dentro del alcance descrito. Se entregan dos ediciones completas, en español
y en inglés. **Cada una tiene 8 páginas en total, 7 figuras originales, 4 tablas y 10 referencias**.
Revisión documental: 24 de octubre de 2026. Las campañas científicas conservan su revisión
histórica. El informe [`paper-audit.json`](evidence/paper-audit.json) para español y
[`paper-audit-en.json`](evidence/paper-audit-en.json) para inglés registran **más de 200
comprobaciones automáticas aprobadas por idioma**; el recuento exacto queda registrado en cada JSON.
Se revisaron además las 16 páginas renderizadas de las dos ediciones finales.

## Formato y estructura

El modelo solicitado es [Attention Is All You Need](https://arxiv.org/pdf/1706.03762). Se descargó e
inspeccionó su PDF v7: papel Letter de 612 x 792 puntos, una columna de 396 puntos, cuerpo
Times/Nimbus Roman de aproximadamente 10 puntos, título centrado entre reglas, resumen sangrado,
secciones numeradas y tablas con reglas horizontales.

C Ket utiliza Times New Roman embebida de 10 puntos, títulos principales de 12 y título de 17,2;
STIX sirve de alternativa si Times no está instalada. El bloque de texto conserva 108 puntos de
margen lateral. Una reserva de 2 puntos en párrafos compensa pequeñas diferencias de métricas al
combinar texto serif y código. Se comprobaron los límites geométricos de cada página, los números
centrados y los enlaces externos. Las referencias están completas en la página 8: no hay anexos
fuera del límite.

La progresión conserva siete secciones: introducción, antecedentes, arquitectura, motivación y
costos, configuración experimental, resultados y conclusiones, seguidas de referencias. Los
apartados específicos de modelos de aprendizaje se adaptaron al estudio de un compilador. El título,
autoría, texto y figuras corresponden a C Ket; no se copiaron figuras, afiliaciones ni marcas del
artículo de referencia. Las decisiones están registradas en
[`layout-reference.json`](evidence/layout-reference.json). La versión extensa anterior se conserva
en [`manuscript-before-format.md`](evidence/manuscript-before-format.md).

La versión inglesa conserva la misma estructura, cifras, contratos y límites. Se traducen también
diagramas, ejes, leyendas, captions, tablas y notas de referencias; los títulos oficiales de los
artículos se conservan. Los bloques C y JSON, las tres ecuaciones, las cifras del cuerpo y los diez
enlaces bibliográficos coinciden entre idiomas. Esta equivalencia se verifica de forma automática
además de la revisión editorial de la traducción.

## Revisión de la redacción académica

Se reescribieron resumen, contribuciones, métodos, discusión, conclusiones y captions en ambos
idiomas. Los documentos presentan el sistema, su evaluación y sus resultados sin describir el
proceso de edición o mantenimiento del repositorio. Se eliminaron la sección de disponibilidad, la
atribución tomada de la licencia, las rutas de evidencia, las instrucciones de auditoría, las notas
sobre el checkout y las referencias a metas internas de desarrollo. La autoría se conserva en la
cabecera y la referencia al software en la bibliografía. La revisión exacta y los procedimientos de
reproducción siguen documentados en esta auditoría y en el README del paper.

Se conservaron las cuatro tablas, siete figuras, tres ecuaciones y diez referencias, así como las
cifras del estudio. La auditoría comprueba también la ausencia de notas editoriales internas en
fuentes y PDF, y restringe los bloques del manuscrito a los ejemplos C y JSON que explican la
arquitectura. El texto se distribuye de forma continua; únicamente las conclusiones y la
bibliografía comienzan en una página nueva. Los ejemplos de código se mantienen íntegros y unidos a
sus párrafos introductorios. La auditoría verifica que ningún bloque C o JSON quede repartido entre
páginas. También comprueba que ningún título termine una página separado de su contenido.

## Alcance de la evidencia

Los resultados experimentales pertenecen al estado fijado
`1ebb2e308e61bed5632798ded5beecf77b6b9a9c`, cket 0.4.0 experimental: GCC 11.4.0, Ubuntu 22.04 y WSL2
x86_64. Esta revisión de formato conserva los logs del estudio anterior; no presenta sus campañas
como nuevas ejecuciones. Los ejemplos C y JSON exactos del manuscrito sí se volvieron a ejecutar en
el checkout actual.

| Evidencia del estudio fijado   | Resultado                 | Archivo                               |
| ------------------------------ | ------------------------- | ------------------------------------- |
| Reconstrucción del proyecto    | Aprobada                  | [build.log](evidence/build.log)       |
| Suite nativa                   | 354 aprobadas, 0 fallidas | [native.log](evidence/native.log)     |
| 16 ejemplos, cinco niveles GCC | 426 comprobaciones        | [native.log](evidence/native.log)     |
| Frontend diferencial           | 145 compilaciones         | [frontend.log](evidence/frontend.log) |
| Síntesis sin tablas            | 24 circuitos              | [growth.log](evidence/growth.log)     |
| OpenQASM contra QuEST 3.7.0    | 11 circuitos              | [qasm.log](evidence/qasm.log)         |

El checkout se amplió posteriormente a 36 ejemplos y recibió una corrección del compilador. Sus
pruebas, incluidos sanitizers, son una campaña diferente y no se mezclan con las 426 comprobaciones
del estudio fijado. Para reproducir esa tabla se debe usar la revisión indicada en un checkout
separado.

Las cantidades de campañas distintas no se suman como cobertura homogénea. Las 145 compilaciones
cubren 29 casos en cinco niveles; las campañas internas de 1024 expresiones y 4096 mutaciones son
partes de casos agregados. En QuEST, ocho circuitos comparan estados hasta fase global con
tolerancia `1e-10`; tres canales comparan 8192 transcripciones por circuito e implementación. El
importer independiente cubre el subconjunto OpenQASM emitido.

## Verificaciones de esta edición

Se comprobaron contra el código y los contratos locales el atributo `quant`, el evento
`PLUGIN_PRE_GENERICIZE`, la IR JSON propia, las dependencias de descomputación, la ruta híbrida, la
síntesis en la primera llamada y los límites de 64 bits, 128 qubits representados y 24 simulados.

Los tres ejemplos C del manuscrito se compilaron y ejecutaron con semilla 7: el programa completo
imprime 12; el fragmento Bell da dos bits iguales; la búsqueda imprime 3. Los fragmentos se
envolvieron en un `main` e includes, sin alterar sus operaciones. La instrucción JSON se validó y su
estado representa exactamente 7+5 en el destino. Estos checks de semilla no prueban por sí solos la
distribución probabilística: se contrastaron además los estados almacenados de Bell y Grover.

Las figuras 1 y 2 son esquemas. Bell tiene probabilidad 1/2 en 00 y 11. Grover usa N=16, M=1 y tres
iteraciones; su probabilidad `0.9613189697265625` coincide con el estado del backend, con
normalización y auxiliar limpio. La memoria de la figura 5 es teórica: 16 bytes por amplitud, 256
MiB a 24 qubits y 64 GiB a 32, excluyendo otras estructuras del proceso.

Las figuras 6 y 7 y cada celda de las tablas de puertas y workspace se verificaron contra las 24
filas de [synthesis-growth.csv](evidence/synthesis-growth.csv). Las preparaciones están incluidas;
el resultado final permanece sin limpiar. MCX y profundidad se mantienen como recursos lógicos,
anteriores a descomposición y mapeo físico. Los siete PNG superan 1500 píxeles de ancho y cuentan
con una versión SVG.

Las diez referencias se conservan citadas en el cuerpo y enlazadas en el PDF. Sus fuentes primarias
se verificaron durante la elaboración del estudio: documentación oficial de GCC y OpenQASM,
artículos/preprints de Bennett, Grover, BBHT, Silq, Quipper, OpenQASM 3 y QuEST. La atribución de
autor procede de LICENSE y se presenta en la cabecera del artículo. La referencia al software
identifica la implementación estudiada; su procedencia y revisión se documentan por separado del
manuscrito.

## Revisión visual y trazabilidad

Ambos PDF finales se renderizaron con PyMuPDF a 144 dpi y se inspeccionaron completos. Se
verificaron en español e inglés:

- Apertura: título, reglas, autor, resumen e introducción.
- Arquitectura: diagramas, frontend y programa C completo.
- Control coherente: ecuación, descomputación, Bell, código y captions.
- Síntesis y búsqueda: JSON, fórmula y curva de Grover.
- Costos y métodos: memoria, límites y configuración experimental.
- Resultados: validación, curva de puertas y tablas.
- Discusión: registros auxiliares, profundidad y amenazas a la validez.
- Página final: conclusiones, trabajo futuro y las diez referencias.

La distribución precisa de apartados varía con la longitud de cada idioma; ambas ediciones conservan
la misma secuencia y ocho páginas totales.

No quedaron páginas de desbordamiento, tablas partidas, captions separados de sus imágenes, glifos
ausentes ni texto cortado. Se corrigió la notación ket de Bell para utilizar glifos matemáticos y se
recalculó el espacio de etiquetas de gráficos después de cambiar la tipografía. Se abreviaron las
etiquetas de escala logarítmica en ambos idiomas para evitar recortes del eje vertical.

El [manifiesto SHA-256](evidence/sha256-manifest.json) vincula ambos PDF finales, fuentes, figuras
en los dos idiomas, datos, logs, herramientas editoriales y archivos locales relevantes. Los hashes
de archivos de código corresponden al checkout actual; los logs y el identificador de revisión
describen el estado del estudio. El manifiesto permite detectar cambios posteriores y no equivale a
una firma digital o auditoría externa.

La revisión acredita fidelidad documental dentro de este alcance. No es revisión por pares,
demostración formal ni cobertura exhaustiva del compilador. No incorpora nuevas mediciones de
tiempos, consumo físico, ruido o ventaja computacional. Las conclusiones y los contratos
probabilísticos mantienen esas limitaciones.

Las conclusiones de ambas ediciones priorizan el núcleo cotidiano de C11 con contratos cuánticos
explícitos; la biblioteca completa queda como ampliación. La evaluación de 0.4.0 sigue describiendo
un subconjunto histórico; no certifica la cobertura final ni la implementación cuántica de toda
libc. El [inventario](../c11-inventory.md) y la [hoja de ruta](../c11-roadmap.md) clasifican las
restricciones pendientes.

## Revisión del 24 de octubre de 2026

Se corrigió en ambas fuentes y PDF una afirmación falsa: 8 bits relevantes no superan un umbral de
12\. Solo las cargas de 16, 24 y 32 bits lo superan; la de 8 se ejecutó con `--no-tables` por
elección del benchmark. El verificador consulta las constantes del objeto Git de la revisión
estudiada y comprueba la frase tanto en la fuente como en el PDF. Cuatro regresiones incluyen las
dos frases originales incorrectas y el caso de igualdad con el límite.

La auditoría ya interpreta la entrega nativa actual: verifica fuentes y artefactos, presencia y
resultados de los comandos, instrumentación, estados complejos y las inversiones completas de
puertas archivadas. Sus 224 puntos y 56 inversas conservan el alcance finito de esa campaña; no son
una nueva ejecución de toda la suite. Los tres ejemplos exactos del paper se ejecutan además con el
compilador actual, y Bell y Grover contrastan todos sus estados complejos con fórmulas.

Las cifras, tablas, figuras y once artefactos científicos del estudio histórico se conservan. Los
scripts rechazan capturas en la revisión actual antes de escribirlos. La campaña histórica registra
ahora el GCC/G++ seleccionado y exige su versión documentada. Los límites del resumen y las
restricciones se atribuyen expresamente al prototipo evaluado.

La [revisión de afirmaciones y referencias](re-audit-2026-10-24.md) conserva también las
limitaciones de disponibilidad pública y consulta de fuentes. Los informes actualizados y el
manifiesto identifican los documentos entregados y la revisión del código; no son revisión por pares
ni una prueba de corrección universal.

La comprobación de bytes de Git detectó además un AST temporal ignorado y una copia de un informe
histórico desactualizada en el entorno de auditoría. Se excluye el temporal según `.gitignore` y se
usa el informe publicado, que registra correctamente su fallo de construcción. Los resultados
previos y la discrepancia quedan conservados; el manifiesto final se contrasta con los objetos Git.
