# Retornos CPU desde llamadores coherentes

27 de octubre de 2026. Ampliación posterior al SDK de 242 entradas. Las decisiones CPU puras de
retorno se componen con el control cuántico del llamador. La
[nota técnica](../../paper/coherent_returns_note.es.md) explica la selección clásica y la fase
global que se vuelve relativa. El [ejemplo](../../../examples/coherent_returns.c) conserva las
salidas y la interferencia esperadas.

## Implementación

El frontend identifica únicamente banderas y ramas privadas de retorno. Permite sus transiciones CPU
sin habilitar efectos CPU visibles ni almacenamiento general del usuario bajo control coherente.
Protege argumentos y predicados mediante la consulta simbólica de controles existente, incluyendo
aritmética y lecturas que no deben ejecutarse en un camino probado inactivo. Las condiciones
cuánticas de retorno siguen rechazadas; no se introduce medición.

## Resultados dirigidos

| Configuración             | Estados complejos | Inversas completas | Fronteras nativas | Rechazos |
| ------------------------- | ----------------- | ------------------ | ----------------- | -------- |
| Normal, cinco niveles GCC | 500               | 250                | 25                | 20       |
| ASan/UBSan O1             | 100               | 50                 | 5                 | 4        |

Las cinco familias usan cuatro etiquetas coherentes, CPU 0/1/2/3/4 y ambas políticas de síntesis.
Incluyen X, fases controladas, ramas y llamadas anidadas, continuación del llamador y ángulos
`double` clásicos. Se comparan todas las amplitudes con fórmulas C y con evolución independiente de
puertas, se exige espacio privado cero y se invierte la secuencia completa antes de la observación
final. Las fronteras comprueban división/carga/índice inválidos bajo control probado inactivo y un
cortocircuito con control posible. Rechazan argumentos con efectos, escrituras CPU observables,
locales del usuario y retornos cuánticos todavía sin contrato.

La matriz anterior de retornos CPU también pasa: 700 ejecuciones, 500 puntos de estado y 250
inversas en normal; 140, 100 y 50 en ASan/UBSan. Las regresiones por configuración conservan 519
bindings, 305 préstamos de lectura, 90 contratos de entrada, 395 del núcleo y 1270 de operandos
controlados. Estos recuentos tienen alcances diferentes y no son cobertura porcentual de C.

[summary.json](summary.json) identifica 244 entradas congeladas y los componentes usados. Se
reconstruye el plugin; las fuentes y binarios del runtime, driver, backend y motor permanecen
iguales. `evidence.tar.gz` conserva fuentes, comandos, trazas, resultados, hashes y los intentos de
preparación separados. El rechazo anterior del llamador coherente sigue en su cápsula histórica;
esta ampliación lo supera para los predicados CPU puros descritos.

El [SDK nuevo](../../../output/sdk/coherent-returns/README.es.md) incluye el ejemplo. Los paquetes
anteriores conservan sus identidades. Quedan integración completa, bucles coherentes, salidas
decididas por datos cuánticos, vidas/alias/ABI generales y flotantes cuánticos. No se afirma una 1.0
estable, QPU física ni aceleración. La ciberseguridad permanece clásica.
