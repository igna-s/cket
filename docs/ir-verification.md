# Validación de la Quantum IR

El verificador de [native/verify.c](../native/verify.c) comprueba la IR serializada antes de sintetizar o ejecutar cada operación. Lo utilizan tanto el runtime enlazado como la validación y exportación de documentos. GCC sigue siendo el único parser de C.

## Esquema y tipos

- Cada operación y expresión tiene un conjunto explícito de campos admitidos. Los campos desconocidos y duplicados se rechazan.
- Las anchuras son enteros de 1 a 64 bits; `signed` debe ser un booleano JSON real.
- Los operadores tienen aridades comprobadas. Se revisan estructuralmente todas las ramas, incluidas las que no se evalúan para un valor concreto.
- La aritmética exige operandos convertidos al tipo común correspondiente. Las comparaciones requieren tipos compatibles; los desplazamientos conservan el tipo del operando izquierdo.
- Las conversiones al tipo del destino deben aparecer explícitamente en la expresión, salvo resultados booleanos utilizados como guardas de un bit.
- Las constantes deben caber en su representación declarada.
- Los controles necesitan registros booleanos disponibles. Se rechazan controles contradictorios y alias entre control y destino.
- `qmeasure`, `qreset`, `qprepare`, `qalloc` y `qrelease` no admiten un campo de control coherente.
- Los ángulos de fase deben ser números representables y finitos.
- Los documentos comprueban formato, versión, campos de cabecera y equilibrio de las marcas de control CPU.

La revisión estructural de una rama no obliga a evaluar su aritmética. La evaluación del dominio conserva el cortocircuito de C: una división de una rama no seleccionada no se ejecuta.

El frontend actualizado emite `signed` en cada `qalloc`. Para compatibilidad, un `qalloc` de la IR v1 que omita ese campo conserva el signo sin especificar: se valida la anchura, pero no se inventa una identidad de tipo C que el documento no contiene. La validación no reconstruye el sistema completo de tipos de GCC desde JSON.

## Pruebas de rango mantenidas por el motor

El motor mantiene, independientemente de los metadatos de la expresión:

- Un valor de base conocido, cuando se puede demostrar.
- Una máscara de bits probados cero.
- Una preparación uniforme intacta y su límite superior.

Un nodo `reg` con `known` solo se admite si coincide con el valor demostrado por el motor. Un `active` reducido solo se admite cuando los bits que omite están probados cero.

La preparación establece esas pruebas. Las actualizaciones las actualizan en los casos de base conocidos o las descartan conservadoramente. El reset establece cero. La descomputación correcta restablece el destino en cero. Una medida conserva los ceros ya probados, pero invalida la preparación uniforme necesaria para `find`.

Las fases y operaciones sobre resultados derivados invalidan, cuando corresponde, la uniformidad de sus entradas. Esto impide que un documento reutilice como entrada intacta de Grover un registro modificado o afectado por una medición de un resultado relacionado.

## Certificado de descomputación

Cada `qcompute` registra:

1. La expresión tipada creadora.
2. La máscara y el patrón de los controles externos.
3. Las identidades y versiones de todas las entradas y controles.

Antes de aceptar `quncompute` se exige la misma expresión, el mismo control normalizado y dependencias disponibles con las mismas versiones. Reordenar la lista de controles no cambia la máscara ni el patrón.

La representación actual compara el texto JSON de la expresión creadora; una expresión equivalente con otra serialización puede rechazarse. Es una restricción conservadora, no una prueba de equivalencia general.

Mientras un resultado necesita una entrada o control para su inversa, el motor rechaza cambiar, medir, resetear o liberar esa dependencia. Las operaciones que cambian los valores de base o el ciclo de vida incrementan la versión.

Una fase diagonal sobre una entrada o control puede conservar la validez de la inversa del cálculo booleano: no cambia los valores de base utilizados por ese cálculo. Se invalida la uniformidad para `find`, pero no se trata esa fase como una sobrescritura del valor. Las pruebas verifican que la fase relativa sobreviva a la descomputación controlada.

Tras la inversa se marca el destino como cero y se elimina el certificado. En simulación se comprueba además que su probabilidad fuera de cero sea menor que `1e-10` antes de reutilizarlo. Esa comprobación numérica complementa la prueba; la exportación no depende de disponer de amplitudes simuladas.

## Operaciones estructuradas y argumentos CPU

`qadd_register` conserva una fuente sin signo y suma/resta modularmente sobre un destino distinto. `qxor_inplace` exige un operando independiente del destino; puede usar una expresión tipada o un valor CPU. `qfourier` admite controles que no se solapen con el registro transformado, rechaza inversas pendientes y descarta pruebas de valor y uniformidad.

`qprepare` con `dynamic: true` recibe un valor ya convertido por C mediante la API de argumentos enteros. Requiere almacenamiento nuevo limpio y no admite simultáneamente límites estáticos. `qphase` dinámico recibe un `double` finito por una API específica; no se confunde con el argumento entero de suma, XOR o preparación. Durante validación se conservan efectos abstractos sin inventar valores; exportación y simulación del documento sin argumentos reales se rechazan.

La síntesis simbólica de bits y predicados no utiliza el límite de 12 bits de las tablas. Antes de emitir puertas se comprueban todos sus operandos; las dependencias y sus versiones se certifican también cuando esa ruta no necesita enumeración. Sus límites defensivos de términos no constituyen una prueba de que todas las expresiones admiten síntesis escalable.

`qindexed` declara una lista de identidades candidatas y una acción sobre el primer candidato. El verificador exige identidades únicas, registros preparados con igual anchura y signo, ausencia de inversas pendientes y una acción admitida sin controles coherentes: suma/XOR con argumento CPU, medida, reset o Fourier. `qc_apply_index` comprueba el índice y aplica la acción únicamente al registro seleccionado. La validación sin índice real comprueba todos los candidatos y descarta pruebas de valor, uniformidad y limpieza: un reset condicional no certifica cero para todos los elementos. No se admite convertir esa selección CPU en un circuito estático.

## Alcance de la garantía

Este verificador comprueba operaciones de la IR y sus dependencias sobre la secuencia procesada. No es una demostración formal del compilador ni un análisis completo de todos los caminos CPU.

Las marcas de ramas y bucles CPU se validan estructuralmente, pero no contienen un grafo de control completo con las condiciones y todas sus rutas. La ejecución híbrida y la exportación estática siguen teniendo los límites documentados.

El frontend comprueba movimientos y préstamos estáticos en las funciones expandidas, incluidos alias y solo lectura. Las reglas generales entre módulos, efectos y vidas dinámicas y la propagación nativa de `quant` en GCC necesitan trabajo adicional. El significado de `qmove` entre nombres se controla en el frontend; la API de registros por identidad no reconstruye la propiedad de todos los nombres de C.

Las pruebas utilizan las APIs públicas en contextos de exportación y simulación, y comprueban controles incorrectos, dependencias vivas, metadatos falsos, tipos, aridades, fases y limpieza. No equivalen a una campaña completa de fuzzing o una prueba formal.

## Circuitos, busqueda y presupuestos incorporados

`qborrow` declara un parametro de modulo con rango desconocido y representacion fija. `qmodule` remapea sus identidades y controla o invierte un cuerpo unitario. Se rechazan alias, representaciones incompatibles, efectos CPU/no unitarios, dependencias invalidadas y trabajo privado que no se libere limpio. Los parametros preceden al cuerpo; la anidacion de modulos tiene limite de 16 niveles.

`qfind` puede declarar `solutions`; si se omite promete una solucion. El numero se valida como contrato, no mediante un censo del dominio. `qsearch` exige un presupuesto de intentos, verifica candidatos medidos y no se exporta estaticamente. La validacion de la IR comprueba el oraculo aun sin conocer el resultado de la estrategia.

`sequence` conserva la evaluacion de expresiones puras descartadas por GCC. El verificador limita profundidad a 128 y tokens de expresion a 100.000; la sintesis simbolica cuenta tambien las comparaciones de terminos dentro de su presupuesto de dos millones de pasos. Los circuitos aritmeticos registran un plan e invierten sus puertas para devolver el trabajo a cero.

Las caches no sustituyen la verificacion de cada llamada. Sus claves conservan expresion, soporte probado, representacion de cables y controles; el trabajo privado de un plan no puede solaparse con registros vivos. El formato permanece en version 1 con nuevas operaciones admitidas; una herramienta anterior debe rechazarlas explicitamente.
