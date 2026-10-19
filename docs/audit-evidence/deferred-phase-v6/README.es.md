# Ángulos diferidos de phase: candidato v6

Estas 143 entradas congeladas todavía no están integradas. En normal y ASan/UBSan pasan 3380
comprobaciones por configuración: 1540 ejecuciones del runtime nativo, 1540 estados completos de
programas compilados, 220 diagnósticos de dominio, 20 controles anidados y 60 comprobaciones de
esquema y dominio perezoso. Los estados revisan las fases relativas y los cables físicos liberados,
que deben quedar en cero.

Una prueba adicional conserva 400 capturas de circuitos con valores CPU reales por configuración y
400 ejecuciones C ordinarias que calculan los bits exactos de los ángulos con la misma optimización
y modo de builtins. Esas referencias C no usan el plugin ni los sanitizadores. Ambas políticas de
síntesis pasan 800 estados complejos independientes por configuración: 1600 en total. No son
mediciones de hardware.

Se conservan los fallos anteriores. V2 pasa sus 240 filas normales, pero solo 70 de 240
instrumentadas: 170 compilaciones rechazan las comprobaciones de división que inserta GCC. V6
serializa la operación entera tipada y sustituye ese guard por las comprobaciones de dominio del
backend, aplicadas únicamente al dominio seleccionado. V4 y V5 fallaron por el comprobador: primero
supuso que no había cables físicos liberados; después intentó leer puertas sin activar su registro.
El compilador se mantuvo idéntico entre V4, V5 y V6.

La primera prueba independiente de V2 terminó con error al intentar leer un cierre instrumentado
todavía ausente; sus 800 estados normales no convierten ese proceso en una ejecución global exitosa.
La primera regresión instrumentada quedó interrumpida; la continuación conservó su resultado real
fallido. Todos esos registros permanecen separados.

La regresión amplia de V6 sigue en curso en la fotografía archivada y debe terminar antes de aplicar
el código. Siguen pendientes las cargas CPU inválidas, long double, llamadas reales generales y C11
completo. La aritmética de estos ángulos es clásica; no implementa registros cuánticos float o
double. La ciberseguridad permanece clásica.
