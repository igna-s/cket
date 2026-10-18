# Revisión v5 de los operandos controlados

El [resumen](summary.json) identifica las 138 entradas del candidato y conserva las pruebas
específicas. V5 evita guardar versiones innecesarias cuando una actualización controlada usa
literales o capturas escalares puras. Mantiene la prueba del dominio seleccionado cuando un cálculo
CPU debe evaluarse de forma diferida. Así corrige los dos fallos de la
[regresión de v4](../controlled-operands-regression-failure/failure-summary.json).

Pasan 393 casos nativos y 1270 controles de operandos por configuración, normal y ASan/UBSan. La
reconstrucción normal es nueva. El plugin y los validadores instrumentados se compilaron de nuevo,
reutilizando driver, runtime y motor instrumentados de fuentes idénticas, con hashes explícitos. No
se presentan como una reconstrucción instrumentada completa de esta prueba específica; la campaña
amplia separada sí incluye ambas reconstrucciones completas.

Las pruebas independientes de operandos se repitieron sobre v5: 4000 estados complejos e inversas
por frontend, 8000 en total. Su máximo de 34 cables pertenece a exportación y simulación dispersa,
sin una afirmación de ejecución densa nativa de 34 qubits. Las capturas CPU interceptan las llamadas
cuánticas; las ejecuciones nativas son controles separados.

Además, el XOR booleano y los bucles estáticos pasan 300 ejecuciones nativas y 600 estados por
configuración: 600 ejecuciones y 1200 estados en total, cinco niveles de optimización, ambos modos
de builtins, controles base y coherentes, fases de .48 radianes e inversas a nivel de C. Se
comprueban todos los registros vivos y los cables privados en cero. El controlador original terminó
con código 1 porque buscó el resultado instrumentado demasiado pronto, después de cerrar todos los
casos normales. Ese fallo se conserva; solo se ejecutó la continuación instrumentada, que terminó
con código 0. No se inventa un código exitoso para el proceso original.

La [matriz de cargas CPU](pending-cpu-loads.json) sigue mostrando un bug: siete perfiles, cinco
niveles y ambos modos de builtins sobre el integrado y v5 dan 140 ejecutables aceptados que fallan,
mientras las 70 referencias C terminan bien. Incluye asignación, XOR, suma, `memset`, `phase`,
condiciones y un índice con división por cero dentro de una rama inactiva. Estos fallos no se
cuentan como soporte ni como rechazos correctos.

Esta cápsula conserva el estado de las pruebas específicas previo a integrar. Posteriormente, la
[regresión amplia](../controlled-operands-full/summary.json) terminó correctamente y la
[integración](../controlled-operands-integration/integration-summary.json) aplicó las mismas 138
entradas con verificaciones de instalación separadas. Permanecen abiertos `phase`, las cargas CPU,
la ABI general y C11 completo. La ciberseguridad permanece clásica.
