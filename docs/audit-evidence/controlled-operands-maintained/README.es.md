# Suite recurrente de operandos controlados

El [resumen](maintained-summary.json) conserva 138 entradas exactas y una suite nueva de C. Pasan
1270 comprobaciones en normal y otras 1270 con ASan/UBSan: 1000 ejecuciones nativas definidas, 200
diagnósticos de división fuera del dominio, 60 diagnósticos de efectos CPU y 10 ejecuciones de
funciones privadas homónimas por configuración. Las 100 referencias C ordinarias por configuración
se registran aparte. Se prueban cinco niveles de optimización y ambos modos de builtins. La suite
comprueba correlaciones medidas; la prueba de fases completas pertenece a la
[cápsula experimental](../controlled-operands/experimental-summary.json).

Los validadores C se compilaron de nuevo. Estas pruebas específicas reutilizaron herramientas
normales e instrumentadas de v4 cuyos hashes quedaron registrados; no se presentan como dos
reconstrucciones nuevas de todo el compilador. La preparación conserva también un fallo del script
de inserción y su recuperación antes de ejecutar la suite.

La [regresión amplia posterior](../controlled-operands-regression-failure/failure-summary.json) sí
reconstruyó el compilador normal, pero se detuvo en la primera suite nativa con 391 casos exitosos y
dos fallos. No llegó a reconstruir el compilador instrumentado de esa campaña. **El candidato no
está integrado.** Faltan esos dos casos, la regresión completa, la integración y los pendientes de
`phase`, cargas CPU inválidas y C11. La ciberseguridad permanece clásica.
