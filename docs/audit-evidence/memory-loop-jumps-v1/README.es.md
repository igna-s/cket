# Memoria cuántica con saltos CPU: máscaras, estados y coherencia

27 de octubre de 2026. La [nota técnica](../../paper/memory_loop_jumps_note.es.md) explica los cinco
perfiles y cinco formas de control. Se reutilizan exactamente los productores normales e
instrumentados del compilador `5f12080`; no se cambia código de producción ni se reconstruyen
binarios. El [SDK](../../../output/sdk/loop-jumps/README.es.md) conserva su identidad anterior.

| Configuración | Puntos de estado | Inversas completas | Densidades del bit intacto | Mutantes rechazados |
| ------------- | ---------------- | ------------------ | -------------------------- | ------------------- |
| Normal O0/O2  | 4740             | 1200               | 948                        | 1                   |
| ASan/UBSan O1 | 2370             | 600                | 474                        | 1                   |

Las matrices combinan `memcpy`, `memmove` en ambos sentidos, `memset` y escritura de byte alto;
`break`, `continue`, su combinación y control `while`/`do while`; tres selectores de longitud,
cuatro selectores de salto y ambas políticas de síntesis. Las vistas completas usan conteos 0/1/2;
la vista parcial solo 0/1. Los datos están correlacionados con una entrada cuántica y un préstamo
del valor inicial recibe una fase posterior.

Las máscaras esperadas se calculan desde los rangos C. El byte bajo de la palabra parcial contiene
un bit independiente en superposición con fase. Se contrastan estados conjuntos, evolución
independiente de puertas y una matriz reducida 2×2 de ese bit, además de invertir la secuencia
completa. El mutante añade una copia de base a un auxiliar y debe perder la coherencia reducida
esperada. No hay medición/reset antes de los puntos de estado.

La campaña final congela 245 entradas: se añade el validador y cambian únicamente tres scripts de
integración/archivo frente a las 244 de `5f12080`. El archivo exige los marcadores y resultados de
ambas configuraciones, con recuentos propios; cuatro transcripciones incompletas, duplicadas o
contradictorias se rechazan. Se conserva el requisito anterior de fuzzing ampliado del archivador
general, separado del perfil de fuzzing predeterminado del CI.

[summary.json](summary.json) identifica fuentes, binarios reutilizados, comandos y hashes.
`evidence.tar.gz` conserva la instantánea de 245 entradas, programas, trazas, matrices y
comprobaciones del parser y sintaxis de integración. Los intentos preliminares se distinguen de la
campaña final; un choque de nombre de directorio se resolvió conservando la pasada normal terminada
y continuando la configuración instrumentada con otro directorio de herramientas.

Estos resultados califican programas y estados finitos sin llamador coherente exterior. Quedan esa
composición, vidas/alias/ABI generales, otras interfaces de libc, estados arbitrarios y el CI
completo de estas fuentes. No se afirma todo C11, 1.0 estable, QPU física ni aceleración. La
ciberseguridad permanece clásica.
