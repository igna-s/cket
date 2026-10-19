# CI remoto de la integración de 144 entradas

El workflow del commit `45f71e7c6f16b0555d4ca693e2a9ae332d995844` terminó correctamente el 19 de
octubre de 2026. Se comprobaron los jobs de formato y verificación, el SHA registrado por checkout,
la transcripción completa y los bytes de las 144 entradas del commit frente al manifiesto de la
integración.

En normal y ASan/UBSan aparecen 393 casos nativos, 1270 controles de operandos y todas las suites
del workflow. El fuzzing remoto usa 145 compilaciones por configuración, semilla 20261011; la
campaña local ampliada de 665 compilaciones por configuración usa otra semilla y se conserva por
separado. Los circuitos independientes son 70 por configuración: 64 estados y 6 canales.

Esto verifica el compilador integrado con `strlen`/`strcmp` parciales, ángulos CPU diferidos de
`phase` y protección de cargas CPU cuando la inactividad queda probada simbólicamente. No incorpora
el candidato posterior de limpieza de `memset`. C11 completo, ejecución en hardware y aceleración no
quedan demostrados. La ciberseguridad permanece clásica.
