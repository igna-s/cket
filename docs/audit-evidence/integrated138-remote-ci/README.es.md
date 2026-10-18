# CI remoto de la integración de 138 entradas

El workflow del commit `08b9f3c9d4b27a3ec7ad334a50b3d03ee898a1c0` terminó correctamente el 18 de
octubre de 2026. Se comprobaron los jobs de formato y verificación, el SHA registrado por checkout,
la transcripción completa y los bytes de las 138 entradas del commit frente al manifiesto de la
integración.

En normal y ASan/UBSan aparecen 393 casos nativos, 1270 controles de operandos y todas las suites
del workflow. El fuzzing remoto usa 145 compilaciones por configuración, semilla 20261011; la
campaña local ampliada de 665 compilaciones por configuración usa otra semilla y se conserva por
separado. Los circuitos independientes son 70 por configuración: 64 estados y 6 canales.

Esto verifica el compilador ya integrado. No incorpora los prototipos posteriores de `phase` ni
corrige las cargas CPU inválidas en ramas inactivas. C11 completo, ejecución en hardware y
aceleración no quedan demostrados. La ciberseguridad permanece clásica.
