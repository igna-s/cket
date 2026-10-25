# Propiedad, copia e inversas de datos cuánticos

El compilador no puede producir dos copias independientes de un estado cuántico arbitrario
desconocido. El
[teorema de no clonación](https://quantum.cloud.ibm.com/learning/en/courses/basics-of-quantum-information/quantum-circuits/limitations-on-quantum-information)
limita las operaciones físicas; el lenguaje debe respetarlo.

## Los contratos que aplica el código

| Operación                    | Contrato                                                                                                                                                       |
| ---------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `quant T y = x`              | Transfiere la propiedad de los mismos cables. Usar `x` después es un error. Un parámetro prestado no puede transferirse.                                       |
| Préstamo o alias             | Referencia el almacenamiento del dueño. Las escrituras requieren un préstamo mutable y los límites comprobados del objeto.                                     |
| `y = f(x)` con destino nuevo | Calcula una función de los valores de base y conserva las dependencias necesarias para su inversa. Puede entrelazar resultado y entrada.                       |
| `memcpy` / `memmove`         | Copia valores de base dentro del contrato de memoria. Un destino conocido se transforma reversiblemente; sobrescribir un estado desconocido conserva historia. |
| Sobrescritura                | Conserva la versión anterior cuando hace falta para mantener reversibilidad o limpiar valores que aún dependen de ella. La historia consume recursos.          |
| Lectura con índice CPU       | Captura el índice usado en ese momento. Cambiar después la variable CPU no cambia la inversa ya guardada.                                                      |
| Lectura con índice cuántico  | Comprueba el rango y calcula una selección coherente, con auxiliares privados e inversa.                                                                       |
| `measure`                    | Frontera explícita hacia CPU. Medir una expresión calcula y observa su resultado; observar una selección no equivale a liberar todos los elementos del array.  |

Por ejemplo, copiar con CNOT hacia un bit en cero transforma

```text
(α|0> + β|1>)|0>  →  α|00> + β|11>
```

El resultado está correlacionado. No contiene dos estados independientes iguales al original.
Conservar los valores de base de la fuente tampoco promete conservar su estado local puro mientras
está entrelazada con el resultado. Deshacer la copia puede devolver el auxiliar a cero y conservar
las fases introducidas durante el cálculo.

Los bucles CPU pueden recorrer buffers fijos y acumular o restar expresiones enteras independientes
en un dueño sin signo estable. El motor calcula el operando, actualiza el destino y ejecuta la
inversa exacta del cálculo privado. No necesita conservar una historia nueva por cada iteración de
esa actualización. Las operaciones que sobrescriben información siguen sujetas a sus contratos de
historia y vida.

Las [versiones por iteración](c11-runtime-history.md) permiten además sobrescrituras escalares,
resultados de incremento del heap y copias heap→fijo dentro de bucles CPU. Cada captura conserva su
inversa concreta. El cambio de dueño es transporte de identidad; el nuevo valor se calcula con
puertas y conserva la información anterior. La historia puede agotar el presupuesto de qubits.

## Qué comprueba la validación

[`validate-ownership.py`](../scripts/validate-ownership.py) enlaza el runtime real mediante un
[observador de pruebas](../tests/ownership_trace.c). Un intérprete independiente de puertas compara
el estado conjunto complejo completo con el simulador nativo. Las pruebas incluyen fases,
movimientos, copias correlacionadas, historia de sobrescrituras, índices guardados, recorridos,
punteros y conversiones decimales.

Para copias, movimientos y consumidores puros se compara además con fórmulas de estado explícitas,
incluidos los auxiliares en cero. Para escrituras con historia y conversiones se comprueban valores
públicos y fases, además de la evolución completa de puertas. La codificación privada de la historia
no se prescribe. Después de limpiar las conversiones decimales se exige cero en todos los cables
privados. `atol` y `atoll` se ejercitan con consumidores de 8 bits; no se certifican todas sus
salidas de 64 bits.

La inversión independiente del flujo completo de puertas es una comprobación adicional. La limpieza
que realmente emite el compilador se comprueba en sus propios puntos de estado. Los contadores del
runtime deben registrar cero mediciones y resets antes de esos puntos. Los casos negativos exigen
diagnósticos semánticos: un fallo del proceso no cuenta como rechazo correcto.

La [campaña archivada](audit-evidence/ownership-contracts-v1/README.es.md) identifica fuentes,
configuraciones, resultados y límites. Es evidencia finita en simuladores. No completa el perfil de
C cotidiano, todas las APIs de memoria, todos los estados posibles ni ejecución en una QPU física.

## Corrección de promociones enteras

La validación encontró que GCC había distribuido una conversión a byte sobre una suma antes de
entregar el árbol al plugin. `quant uint8_t x = 1; quant int y = INT_MAX; x += y;` acababa con una
comprobación de suma de 8 bits que ocultaba el desbordamiento de `int`.

La nueva ruta recupera las promociones conservadas en los operandos de sumas, restas y productos,
comprueba la operación tipada completa y aplica después la conversión del resultado. Los casts
explícitos se conservan: `x += (uint8_t)y` tiene otro dominio y sigue siendo válido. No se atribuye
esta corrección a todas las simplificaciones posibles de GCC ni a todos los operadores de C.

## Lo que falta para cerrar el plan

Quedan combinaciones generales de funciones, punteros, alias y flujo; almacenamiento y vida de
vectores dinámicos; operadores y recursos fuera del dominio actual; `float`/`double` cuánticos;
integración del perfil completo y una nueva auditoría de los papers. La ciberseguridad permanece
clásica. El [perfil acordado](perfil-c-cotidiano.es.md) define el cierre, sin exigir todo C11.

## Memoria dinámica y no clonación

Los [heaps nativos](c11-heap.md) separan la dirección clásica del estado almacenado. `realloc`
conserva identidades de registros, sin copiar amplitudes. Una escritura puede conservar una versión
anterior para los resultados todavía vivos. `free` retira el acceso público y conserva los estados
desconocidos como salidas conjuntas; los conocidos se limpian reversiblemente cuando terminan sus
préstamos. No equivale a reset ni a medir. El programa debe presupuestar también esa historia
retenida.

## Hallazgos posteriores de coherencia

La [revisión del 24 de octubre](re-audit-2026-10-24-coherence.md) encontró pérdidas de coherencia
por historia redundante y por limpieza incompleta, aun con cero mediciones y resets. Se corrigen los
casos directos observados de buffers fijos y las identidades comprobadas del heap, con
[evidencia de fases y estados conjuntos](audit-evidence/heap-identities-v1/README.es.md). Siguen
abiertos los préstamos tras escrituras que pueden activarse y las copias coherentes no idénticas al
heap. La [limpieza ante inactividad demostrada](audit-evidence/inactive-borrows-v1/README.es.md)
conserva ahora la receta y deshace el temporal al salir del bloque, con su fase esperada. Los
contratos anteriores y sus campañas finitas deben leerse con estos contraejemplos. La unitariedad
del flujo completo conserva información global, pero también hay que comprobar el estado lógico y
las correlaciones que cada operación promete conservar.
