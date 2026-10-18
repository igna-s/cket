# Operandos CPU bajo control cuántico: pruebas separadas

Esta cápsula conserva cuatro prototipos y sus fallos, con 137 entradas congeladas por versión. La
versión v4 pasa 1000 ejecuciones nativas definidas y 200 diagnósticos de división fuera del dominio
por configuración, normal y ASan/UBSan. Hay 100 ejecuciones C ordinarias separadas por
configuración. Además pasan 60 diagnósticos de efectos CPU y 10 ejecuciones de funciones privadas
homónimas por configuración. Los diagnósticos son rechazos comprobados; no cuentan como soporte.

La prueba independiente comprueba 4000 estados complejos e inversas por frontend, 8000 en total, a
partir de capturas reales de sus enlaces CPU. Usa exportaciones de dos políticas de síntesis y
simulación dispersa independiente, con un máximo de 34 cables físicos. Estas capturas no ejecutan el
motor cuántico nativo. El límite de este último sigue en 24 qubits; no se afirma una ejecución
nativa densa de 34 qubits ni una ejecución en hardware cuántico físico.

Las versiones anteriores conservan sus fallos de compilación, metadatos instrumentados y semántica.
La v4 resuelve los seis perfiles de operandos enteros de su matriz; `phase` y una lectura CPU
inválida en una rama inactiva siguen fallando, como muestra `pending-after-v4.json`.

El [resumen](experimental-summary.json) vincula fuentes, registros y circuitos por SHA-256. El
[candidato recurrente](../controlled-operands-maintained/maintained-summary.json) añade una suite C,
pero su [regresión amplia falló](../controlled-operands-regression-failure/failure-summary.json).
**La corrección nueva no está integrada.** La ciberseguridad permanece clásica.
