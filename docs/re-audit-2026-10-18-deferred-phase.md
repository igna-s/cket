# Ángulos de `phase` bajo control cuántico: candidato sin integrar

El compilador integrado conserva el fallo de evaluación anticipada de ángulos como
`if (b) phase(10 / k)` cuando el control está desactivado y `k` vale cero. El candidato v6 contiene
una corrección y pruebas cerradas sobre **143 entradas congeladas**. Su regresión amplia
[terminó con un fallo](audit-evidence/deferred-phase-v6-broad-failure/README.es.md); estos
resultados no certifican una integración. La fotografía inicial conserva su estado histórico «en
curso».

La [cápsula verificable](audit-evidence/deferred-phase-v6/summary.json) conserva las fuentes,
controladores, transcripciones, circuitos y antecedentes fallidos. El árbol integrado sigue
identificado por la
[integración anterior de 138 entradas](audit-evidence/controlled-operands-integration/integration-summary.json).

## Qué cambia en el candidato

Los parámetros clásicos puros de `phase` se serializan en una expresión que conserva las
conversiones y operaciones intermedias de enteros, `float` y `double` admitidas. El backend
comprueba primero si las condiciones cuánticas son demostrablemente contradictorias o inactivas. Esa
demostración usa valores conocidos, definiciones guardadas y sus versiones; no consulta amplitudes
ni mide registros. Si no puede demostrar que el control está inactivo, conserva la obligación de
evaluar y validar el ángulo.

Las divisiones y desplazamientos enteros mantienen sus comprobaciones de dominio. Las conversiones
binarias de 32 y 64 bits conservan el redondeo intermedio. El ángulo final debe ser finito. Esta
aritmética prepara un parámetro clásico de una puerta de fase; no implementa registros cuánticos de
punto flotante.

GCC puede insertar comprobaciones UBSan alrededor de una división antes del paso del plugin. El
candidato reconoce los wrappers puros de división y desplazamiento admitidos y conserva la operación
entera tipada. Sus comprobaciones de dominio quedan a cargo del backend en el dominio seleccionado;
no ejecuta anticipadamente el handler de GCC sobre un brazo inactivo. Las operaciones CPU con
efectos o lecturas `volatile` no se convierten por eso en operaciones cuánticas.

## Qué se comprobó

En cada configuración, normal y ASan/UBSan, las reconstrucciones separadas pasaron:

- 1540 ejecuciones del runtime nativo sobre once perfiles de ángulo, controles de base y coherentes,
  cinco optimizaciones y ambos modos de builtins.
- 1540 comprobaciones de estados completos de los programas compilados, con bits exactos del ángulo
  calculados en C, fases relativas, operaciones inversas y todos los cables privados en cero.
- 220 diagnósticos de dominio, 20 controles anidados y 60 comprobaciones de esquema y evaluación
  perezosa.

El total es **3380 por configuración**. Las clases de prueba tienen alcances distintos; las
mediciones nativas no demuestran por sí solas las fases relativas.

La prueba independiente adicional conserva 400 capturas de circuitos con parámetros CPU reales y 400
ejecuciones de referencia C por configuración. Las referencias usan la misma optimización y modo de
builtins; no usan el plugin ni los sanitizadores. Cada captura se exporta con ambas políticas de
síntesis y se simula mediante una implementación separada de H, X y fase. Se comparan **800 estados
completos por configuración, 1600 en total**, y los bits de cada ángulo. Los perfiles incluyen
conversiones de enteros de 64 bits, redondeo de `float`, condiciones CPU y fases inversas. La
evidencia se limita al entorno flotante predeterminado.

## Fallos conservados y trabajo pendiente

V2 pasó 240 filas normales, pero solo 70 de 240 instrumentadas: 170 compilaciones rechazaron los
guards de GCC. También se conservan una ejecución instrumentada interrumpida, su continuación
fallida y el primer controlador de estados que terminó con error al buscar un cierre aún ausente.
Sus resultados parciales no se presentan como ejecuciones globales exitosas.

V4 y V5 fallaron por el comprobador de estados: primero supuso que liberar registros reducía la
dimensión física; luego intentó leer puertas sin activar su registro. V6 verifica toda la dimensión,
exige cero en los cables privados y activa el registro de puertas. El código fuente del compilador
es idéntico entre esas tres versiones; cambiaron las pruebas.

La regresión amplia cerró 56 comandos con éxito y falló en `sanitized-bindings`. La prueba
preexistente `n ? 1 / d : false`, con `n` falso y `d` cero, fue rechazada como efecto CPU. El
control de volatilidad se aplicaba antes de reconocer el wrapper de sanitizadores de GCC. La
corrección posterior está en pruebas; este cierre fallido se conserva junto con sus fuentes y
registros.

Falta aprobar la regresión amplia corregida y aplicar la integración con sus hashes. Quedan
pendientes las cargas CPU inválidas bajo controles inactivos, `long double`, llamadas reales
generales y la compatibilidad completa con C11. No se ha verificado ejecución en hardware cuántico
ni aceleración frente a CPU. La ciberseguridad permanece clásica y sin cambios.
