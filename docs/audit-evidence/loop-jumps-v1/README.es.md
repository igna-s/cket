# Saltos estructurados de bucles CPU

27 de octubre de 2026. La [nota técnica](../../paper/loop_jumps_note.es.md) explica los permisos
privados y la limpieza. `break` y `continue` se refieren al bucle más próximo y conservan las reglas
de incremento y condición de C; no observan un control cuántico.

| Llamador coherente        | Estados complejos | Inversas completas | Fronteras nativas | Rechazos |
| ------------------------- | ----------------- | ------------------ | ----------------- | -------- |
| Normal, cinco niveles GCC | 1900              | 950                | 55                | 60       |
| ASan/UBSan O1             | 380               | 190                | 11                | 12       |

Diecinueve familias usan cuatro etiquetas coherentes, CPU 0/1/2/3/4 y ambas políticas de síntesis.
Las cinco nuevas incluyen X, fases, `for`, `while`, `do while`, bucles anidados, bloques y llamadas
con retornos propios. Se comparan todas las amplitudes con fórmulas CPU explícitas y evolución
independiente de puertas, se exige espacio privado cero y se invierte la secuencia completa antes de
observar. Las fronteras rechazan saltos decididos por datos cuánticos y `continue` a través de
`switch`, y comprueban operandos inactivos y las restricciones anteriores.

La matriz sin llamador coherente pasa 1100 comparaciones nativas con C, 1000 puntos de estado y 500
inversas en normal; 220, 200 y 100 con ASan/UBSan. Las 22 familias nativas incluyen efectos de
condición/incremento, declaraciones posteriores al salto y decisiones del bucle interior. Las diez
familias de estado incluyen sus fases y limpieza. Sus alcances se solapan con campañas anteriores;
los recuentos no son cobertura porcentual de C.

Las regresiones por configuración pasan bindings (519), préstamos de lectura (305), entradas de
funciones (90), operandos controlados (1270) y núcleo (395). El
[SDK](../../../output/sdk/loop-jumps/README.es.md) reconstruye y ejecuta 37 programas, con 1509
comprobaciones normales de ejemplos. Sus tres lanzadores Windows se comprueban mediante WSL sin
montar unidades Windows. El ejemplo nuevo se ejecuta además con cinco entradas, normal e
instrumentado, para comprobar interferencia tras `continue` y `break`.

[summary.json](summary.json) identifica 244 entradas y los productores. Cambian dos archivos de
producción y dos validadores respecto a `e40f782`; solo se reconstruye el plugin. El runtime,
driver, backend y motor conservan sus fuentes y binarios. La cápsula conserva fuentes, comandos,
trazas, hashes, el rechazo anterior y los resultados separados de cada configuración.

Quedan integración completa de estas fuentes, saltos/iteración cuánticos, `continue` a través de
`switch`, efectos CPU anidados, flujo y vidas/alias/ABI generales, flotantes cuánticos y revisión
científica general. No se afirma C11 completo, una 1.0 estable, QPU física ni aceleración. La
ciberseguridad permanece clásica.
