# Valores pendientes e historial condicional: evidencia finita

26 de octubre de 2026. Corrección posterior a `6400aed`; la versión permanece en `1.0.0-rc.3`. La
[nota técnica](../../paper/lazy_values_note.es.md) explica los tres fallos del frontend y las
referencias de historial antiguas que los validadores todavía utilizaban.

Se congelaron 237 entradas. Respecto de la publicación anterior cambian exactamente
`src/quantum_semantics.inc` y cuatro archivos del validador de historial. El plugin y los
validadores se reconstruyeron; motor, runtime y controladores conservan exactamente los binarios
anteriores, porque sus fuentes no cambian. Los hashes y el alcance de esa reutilización quedan en
`summary.json` y en los registros de construcción.

| Comprobación                                |               Normal |         ASan/UBSan |
| ------------------------------------------- | -------------------: | -----------------: |
| Historial                                   | 2332, suite completa | 355, modo dirigido |
| Expresiones                                 |                 3509 |               3509 |
| Núcleo nativo                               |                  395 |                395 |
| Densidades públicas de cadenas              |                  592 |                592 |
| Estados conjuntos independientes de cadenas |                  592 |                592 |
| Inversas completas de cadenas               |                  296 |                296 |
| Preflight de cadenas sin mutación           |                    8 |                  8 |

Las 355 comprobaciones dirigidas incluyen 320 contextos de estado completo. De ellos, 280 contrastan
valores C, fases, clases abstractas de historial, trayectorias independientes de puertas e inversas
unitarias completas; los otros 40 mantienen sus referencias directas estrictas. Las observaciones
explícitas se cuentan aparte y se desactivan para el contraste unitario. No se afirma haber
ejecutado las 2332 comprobaciones bajo sanitizadores.

El bloque 06 también produjo diez ejecutables y diez exportaciones QASM en normal. No se presentan
esas exportaciones como una nueva validación con un SDK independiente.

`evidence.tar.gz` conserva fuentes, identidades, construcción, comandos, registros finales, el
rechazo exacto del programa mínimo en la publicación anterior y la prueba del lanzador Windows.
`earlier-attempts.tar.gz` conserva exploraciones fallidas de referencias y correcciones intermedias;
sus fallos no cuentan como resultados positivos. Los hashes de ambos archivos figuran en
`summary.json`.

La [demo lista para ejecutar](../../../output/demo/README-lazy.es.md) incluye un caso de base antes
rechazado y un programa con entradas en superposición. Sus resultados medidos verifican valores; las
pruebas de coherencia están en las referencias independientes descritas arriba. Es ejecución con
simulador nativo, sin afirmación de ejecución en una QPU.

Las catorce exclusiones por presupuesto de 128 wires de la matriz de cadenas siguen explícitas.
Continúan pendientes la integración general, vidas y alias generales, todas las escrituras
repetidas, flotantes cuánticos y la revisión de los papers generales. Estos recuentos no son un
porcentaje de C implementado ni una certificación de la versión 1.0 estable. La ciberseguridad
permanece clásica.
