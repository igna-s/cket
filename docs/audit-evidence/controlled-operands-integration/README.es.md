# Integración de operandos controlados

El [resumen](integration-summary.json) identifica las 138 entradas aplicadas, la campaña amplia
completa previa y las reconstrucciones separadas de instalación. Pasaron, por configuración, 393
casos nativos, 1270 controles de operandos y 40 ejecuciones de cuatro programas exactos del
contrato, en cinco niveles de optimización y con builtins activados y desactivados.

Los 1270 controles separan 1000 ejecuciones nativas definidas, 200 diagnósticos de dominio, 60
diagnósticos de efectos y 10 ejecuciones de homónimos privados. Las 100 referencias C ordinarias se
registran aparte. Los 8000 estados de operandos y 1200 de XOR/bucles pertenecen a la cápsula
independiente de v5; su alcance y sus fallos de coordinación se conservan.

La aplicación de fuentes y herramientas normales a la copia Linux sigue a esas verificaciones. La
importación de fuentes a Windows comprueba hashes y conserva un respaldo; no constituye una segunda
ejecución de la regresión completa. La semántica del ángulo de `phase`, las cargas CPU inactivas y
C11 completo siguen pendientes. La ciberseguridad permanece clásica y sin cambios.
