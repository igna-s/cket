# Lecturas CPU bajo controles cuánticos: pruebas dirigidas cerradas

Este candidato conserva 143 entradas de fuentes. Todavía no está integrado: el árbol principal
conserva la integración anterior de 138 entradas.

Las matrices normal y ASan/UBSan pasan 140 filas cada una: asignación, XOR, suma, memset, fase,
condición y acceso indexado; cinco optimizaciones, ambos modos de builtins, controles de base y
coherentes. Cada matriz ejecuta 420 programas nativos y 420 referencias C ordinarias, sin
instrumentar estas últimas.

La calificación fresca cierra doce comandos con éxito. En cada configuración pasan 393 pruebas
nativas, 1270 de operandos, 3380 de fase y 76 comprobaciones de la consulta simbólica. Estas últimas
comparan amplitudes exactas, registros, versiones, contadores y RNG antes y después: la consulta no
observa el estado.

V1 conserva 20 fallos reales entre sus 140 filas. V2 pasa esa matriz, pero su prueba de fases falla
porque el capturador no interceptaba la consulta de controles. V3 conserva el mismo compilador que
V2 y corrige ese capturador para consultar el contexto real capturado. Los fallos originales quedan
archivados.

Faltan la prueba independiente de estados exportados específica de estas lecturas, su regresión
amplia y la integración. Los controles que no se pueden demostrar inactivos conservan la obligación
de evaluar la lectura. Esto no certifica toda C11, hardware cuántico ni aceleración. La
ciberseguridad sigue clásica.
