# Predicados, conversiones y negación lógica corregida

El [nuevo cálculo](../../c11-shifts.md#boolean-consumers-and-integer-conversions) obtiene la verdad
de un desplazamiento directamente y limita los resultados intermedios de conversiones enteras. Los
tres casos exactos que antes fallaban por 128 qubits ahora compilan y generan circuito con ambas
políticas de síntesis. El cuarto programa del bloque 02, `shift_predicate`, ejecuta con 85 qubits.

También se corrige un error numérico de la ruta aritmética: una negación con resultado C `int` podía
reducir el operando a su bit bajo. La reproducción sin tablas contiene un desplazamiento y una
conversión a byte; para fuente 2 y contador 0, la biblioteca anterior asigna amplitud al resultado
incorrecto 1. La nueva la asigna al resultado correcto 0, con las mismas entradas y amplitud.

Las 164 entradas congeladas pasan por configuración normal y ASan/UBSan: 203 comprobaciones de
desplazamientos, 3505 expresiones, 393 casos nativos y 990 divisiones. Los 132 nuevos dominios
dispersos comparan estados complejos completos con cuatro etiquetas fuente declaradas por ancho,
todos los contadores del tipo promovido, seis formas booleanas, conversiones a byte, fases,
controles e inversas. No se presentan como todos los valores de entrada de 32/64 bits.

`native-example.json` añade 32 ejecuciones del programa exacto en `-O2`: 16 normales y 16 con
ASan/UBSan, con ambas ramas de entrada observadas y comprobaciones C de los valores medidos. Este
corpus se distingue de las comparaciones coherentes de estado completo.

La construcción normal es nueva y completa. ASan/UBSan reconstruye motor, runtime, driver, backend y
validadores; reutiliza únicamente el plugin GCC instrumentado sin cambios, comprobando su identidad.
Los registros del primer intento conservan el fallo de compilación del generador de JSON; los del
segundo conservan el estado numérico incorrecto que llevó a corregir la negación.

Dos pilotos anteriores de negación directa no alcanzaban la ruta afectada y daban resultados
correctos en la versión anterior. La expectativa del controlador era incorrecta; sus fuentes y logs
se conservan y no se cuentan como fallos del compilador ni como pilotos aprobados. La reproducción
final sí alcanza el fallback afectado y confirma el error tanto en las 161 entradas anteriores como
en el segundo candidato de 164 entradas.

`summary.json` fija fuentes, resultados y alcance. `evidence.tar.gz` conserva los registros, fuentes
actuales y diferencias de los intentos fallidos, reproducciones y circuitos/reportes del bloque. La
regresión amplia y CI de estas entradas, el bloque 02 completo y C11 completo siguen pendientes. Se
mantienen el límite de 128 qubits, el presupuesto de soporte disperso y los límites numéricos de
`double complex`; no se afirma ejecución física ni aceleración cuántica.
