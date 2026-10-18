# Condiciones clásicas dentro del control cuántico

**Esta cápsula conserva el bug del árbol anterior de 134 entradas y sus prototipos.** La
[corrección posterior](../coherent-memory-integration/integration-summary.json) ya se integró sobre
137 entradas con reconstrucciones y pruebas específicas. Su regresión amplia sigue pendiente. Las
pruebas que se aprobaron antes conservan su alcance finito; no demuestran ausencia de bugs.

## Qué fallaba

Con una condición cuántica exterior preparada en cero, una división situada en una condición
interior podía evaluarse anticipadamente en CPU. El programa debía dejar esa rama inactiva. La
referencia en C ordinario termina correctamente; el compilador principal acepta el programa y su
ejecutable termina con la señal de división por cero en las diez configuraciones probadas: cinco
niveles de optimización y builtins activados o desactivados.

El [resumen verificable](experimental-summary.json) identifica las **134 entradas exactas del
compilador principal** que reproducen el fallo. Sus fuentes y los programas completos se conservan
en las cápsulas; el resultado anterior no se reemplazó por el de la corrección.

## Qué cambia el candidato

El nuevo frontend conserva la aritmética de las condiciones anidadas dentro del circuito y mantiene
su evaluación condicionada. También permite que una condición clásica pura actúe como entrada del
predicado cuántico, sin medir el control exterior. Las capturas de valores CPU conservan los
contextos válidos ya admitidos de variables, campos, arrays, índices dinámicos y punteros.

Las escrituras, incrementos y accesos observables sobre CPU dentro de esas condiciones siguen
teniendo límites de efectos explícitos. Este trabajo no modifica la ciberseguridad.

## Qué se comprobó

| Comprobación del último prototipo           | Resultado | Alcance                                                                                                              |
| ------------------------------------------- | --------: | -------------------------------------------------------------------------------------------------------------------- |
| Ejecuciones nativas normales                |       180 | Diez perfiles, cinco niveles y dos modos de builtins, con sus entradas CPU declaradas.                               |
| Ejecuciones nativas con ASan/UBSan          |       180 | Frontend instrumentado reconstruido; componentes del motor sin cambios identificados por hash.                       |
| Rechazos específicos por configuración      |        30 | Efectos sobre CPU que el contrato aún no admite; no se cuentan como compatibilidad implementada.                     |
| Estados complejos e inversas independientes |       800 | Doscientos prefijos CPU concretos, dos políticas de síntesis, antes y después de invertir definiciones y escrituras. |

Las referencias comprueban todos los registros vivos, valores intermedios, controles, amplitudes,
fases y limpieza de temporales. Los circuitos llegan a **43 qubits físicos**, pero las referencias
de estos perfiles tienen una o dos amplitudes: se comprobaron mediante simulación independiente
dispersa. El caso que excedía los 24 qubits del simulador denso conserva su fallo de recursos y no
se cuenta como una ejecución nativa exitosa.

La captura de prefijos intercepta expresamente las llamadas cuánticas de un ejecutable para
registrar sus parámetros CPU reales y se detiene antes de la primera medición. La ejecución del
estado cuántico se comprueba después, de forma independiente. Las 180 ejecuciones nativas de la
tabla son pruebas separadas que sí ejecutan el motor cuántico.

El [validador C recurrente](../coherent-cpu-conditions-maintained/maintained-summary.json) del
candidato posterior pasó **170 comprobaciones normales y 170 instrumentadas**. Incluye 90 casos
nativos, 40 rechazos y 40 circuitos de acción e inversa por configuración; las 45 referencias en C
ordinario se cuentan aparte. El antiguo caso de rechazo de la distancia guardada en una variable
clásica se convirtió en una prueba positiva. Su segunda referencia de fase declara explícitamente
que esa distancia vale uno antes de concretar el circuito.

## Qué falta

La regresión amplia del candidato corregido está en curso. Las pruebas específicas de integración ya
terminaron y están registradas por separado. El fallo descrito arriba corresponde a las 134 entradas
históricas. La integración posterior conserva una prueba positiva del caso corregido.

Los accesos a memoria CPU que no sea válida al construir el circuito requieren trabajo adicional.
Por ejemplo, una lectura mediante un puntero nulo dentro de una rama cuántica inactiva sigue siendo
un caso pendiente reproducido. La corrección de la división no demuestra que ese problema de memoria
esté resuelto.

Las dos primeras versiones del prototipo también conservaron regresiones de lecturas clásicas que la
ampliación de pruebas descubrió. El resumen las identifica como versiones que **no se deben
integrar**, aunque sus matrices iniciales hubieran pasado. La tercera versión conserva los casos
válidos de arrays, campos, índices dinámicos y punteros que se probaron después.

No se afirma compatibilidad completa con C11, ejecución en hardware cuántico ni aceleración frente a
CPU. La ciberseguridad permanece clásica y sin cambios. Este documento explicativo se añadió después
de cerrar las cinco cápsulas originales y no modifica sus hashes.

Los [contrastes posteriores](../coherent-arm-operands/failure-summary.json) reproducen siete fallos
todavía abiertos en operandos del cuerpo de una rama cuántica inactiva. La corrección de condiciones
no se presenta como una solución de todos los operandos ni de toda la memoria.
