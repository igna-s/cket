# Cadenas fijas: corrección de coherencia posterior a RC3

26 de octubre de 2026. Este bloque corrige escrituras de cadenas, con evidencia finita. La versión
sigue siendo `1.0.0-rc.3`; el cierre general de la 1.0 continúa pendiente. La
[nota técnica](../../paper/string_writes_note.es.md) explica las máscaras y la no clonación.

Los contraejemplos de `strncpy` de un byte y `strcpy` vacío reproducen en RC3 una caída de pureza
conjunta de 1 a 0,5. Sobre el compilador corregido, los siete contrastes registrados conservan
pureza 1 y error de densidad cero. La concatenación vacía, antes rechazada por el presupuesto de
síntesis, también compila. No hay mediciones ni resets antes de esos contrastes.

## Matriz de escrituras

| Construcción  | Programas compilados | Densidades públicas | Estados conjuntos por otro intérprete | Inversas completas | Dominios en ejecución |
| ------------- | -------------------: | ------------------: | ------------------------------------: | -----------------: | --------------------: |
| Normal O0/O2  |                  148 |                1184 |                                  1184 |                592 |                    16 |
| ASan/UBSan O1 |                   74 |                 592 |                                   592 |                296 |                     8 |
| Total         |                  222 |                1776 |                                  1776 |                888 |                    24 |

Son ámbitos diferentes. La referencia semántica calcula la densidad pública sin utilizar las
amplitudes observadas como valores esperados. Otro intérprete evoluciona las puertas reales y
comprueba el estado conjunto; después invierte la secuencia completa.

Se incluyen terminadores cuánticos, relleno, fuentes vacías, controles abiertos/anidados, ramas CPU,
productores negativos y fases sobre préstamos anteriores. Las copias y concatenaciones vacías usan
palabras con/sin signo de 16/32 bits. Las concatenaciones no vacías comprobadas usan 16 bits; sus
préstamos comprobados son dos vistas booleanas antiguas de una raíz. Los préstamos de copia
conservan una palabra completa de 16 bits. Catorce combinaciones mayores quedan excluidas
explícitamente por los 128 cables y el espacio de trabajo. No se cuentan como éxitos.

Los casos de NUL inválido desde la CPU verifican que el destino permanezca igual al fallar y que una
guarda inactiva evite evaluar el dominio. Con tablas, el mensaje nombra la guarda interna de
división por cero; sin tablas, nombra la ausencia de síntesis. Un diagnóstico específico de cadenas
y la validación general de capacidades siguen pendientes.

## Regresiones y fallo general abierto

Por construcción pasan 55143 comprobaciones directas de máscaras clásicas/cuánticas, 35871 de
intercambios, 3509 de expresiones, 460 de cadenas de lectura, 16878 de heap y 395 del núcleo. La
matriz de buffers fijos añade por construcción 640 densidades, 640 estados conjuntos y 304 inversas;
se registra aparte de la matriz de cadenas.

`validate_history` falla en su referencia escalar inicial, tanto en RC3 como en ambas construcciones
nuevas. Para `if(c) x=false`, su fórmula toma el mismo dueño como valor anterior y como destino
actualizado, y exige `x=1` en una rama donde C exige `x=0`. Se conservan los tres logs y su firma
exacta. Actualizar esa referencia y auditar sus demás perfiles sigue pendiente. La campaña completa
no se declara aprobada y este bloque no califica el CI general.

El bloque 06 normal termina con diez ejecutables y diez exportaciones QASM. Su primera ejecución
detectó los enlaces estáticos antiguos de `string_copy`; se corrigen de la instrucción 57 con dos
valores a las instrucciones 73 y 77 con un valor cada una. La revisión de ejemplos tiene 237
entradas y difiere de la campaña solamente en ese JSON. Se reutilizan los artefactos del compilador
idénticos byte por byte y se ejecuta el bloque completo de nuevo. No hay una nueva calificación
mediante SDK independiente.

## Fuentes y continuidad

`summary.json` identifica las 237 entradas de la campaña y las 237 de publicación, su única
diferencia y los hashes de herramientas. `evidence.tar.gz` conserva esas fuentes, el JSON anterior,
comandos, logs, símbolos de instrumentación y contraejemplos. Las tentativas anteriores están
separadas en `earlier-attempts.tar.gz`; incluyen fallos exploratorios y un observador de salida
corregido antes de congelar la campaña.

La matriz normal de cadenas terminó antes de una interrupción. La continuación reutilizó en orden
los 33 comandos exitosos de la regresión fija y ejecutó solamente lo pendiente. La primera
construcción instrumentada se interrumpió mientras compilaba el plugin, sin producir artefactos; su
continuación tiene un log separado. Los fallos de referencias y enlaces no se borran del registro ni
se convierten en comprobaciones exitosas.

Siguen abiertos las vidas y alias/ABI generales, cadenas sobre heap, escrituras repetidas en más
contextos, perfiles grandes de 64 bits, flotantes cuánticos, el validador escalar, el CI completo y
la revisión general de papers. Los PDF históricos conservan su revisión. La ciberseguridad permanece
clásica y sin cambios.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
build/validate_patches
build/validate_expressions
python3 scripts/validate-string-writes.py --output build/new-string-writes --levels O0 O2
bash scripts/run-block.sh 06
```
