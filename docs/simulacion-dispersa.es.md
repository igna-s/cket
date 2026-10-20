# Ejecutar enteros anchos con pocos estados ocupados

El runtime nativo incorpora un modo de simulación dispersa. Permite ejecutar circuitos de hasta 128
qubits físicos cuando su soporte cabe en el presupuesto configurado. Por ejemplo, ya se pueden usar
dos `quant int` de 32 bits correlacionados o un `quant size_t` de 64 bits con un resultado pequeño.
La representación de circuitos conserva su límite de 128 qubits.

En Linux/WSL, después de construir el compilador:

```bash
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
bash scripts/run-block.sh 02
CKET_SIMULATOR=sparse build/blocks/02/wide_integers
CKET_SIMULATOR=sparse build/blocks/02/wide_size
CKET_SIMULATOR=sparse build/blocks/02/variable_shift
CKET_SIMULATOR=sparse build/blocks/02/shift_predicate
```

El runner deja cuatro ejecutables, sus circuitos OpenQASM y sus informes de recursos en
`build/blocks/02/`. La variable se debe conservar al ejecutar después un binario individual.

| Programa        | Operación cuántica                                                            | Salidas válidas en el perfil Linux LP64                                                         |
| --------------- | ----------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| `wide_integers` | Selección coherente de un `int` de 32 bits y división por tres en otro `int`. | `control=0 value=10 quotient=3` o `control=1 value=-7 quotient=-2`                              |
| `wide_size`     | Selección de extremos `size_t` de 64 bits y conversión del cociente a byte.   | `control=0 count=9223372036854775808 tail=170` o `control=1 count=18446744073709551615 tail=85` |

El nuevo `variable_shift` calcula `value >> count` con un `uint32_t` y un contador en superposición
de 0 a 31. Su resultado medido debe coincidir con la referencia C aplicada a los operandos medidos.
El [selector de posiciones](c11-shifts.md) evita el límite anterior de auxiliares y su circuito usa
105 qubits.

`shift_predicate` usa un entero de 64 bits y un contador en superposición de 0 a 63. Calcula si el
desplazamiento es distinto de cero y si lo es después de convertirlo a byte, conservando ambas
semánticas. El circuito usa 85 qubits; se miden explícitamente los predicados y sus operandos y se
comprueban sus correlaciones con operaciones C clásicas.

Los dos programas anteriores miden explícitamente al final y comprueban las correlaciones de la
tabla. Sus informes estáticos registran picos de 99 y 84 qubits respectivamente; ejecutarlos no
requiere un vector denso de `2^99` o `2^84` amplitudes.

## Qué hace el motor

Cada entrada almacena una etiqueta de base de 128 bits y una amplitud compleja. Las puertas X
permutan etiquetas, las fases modifican amplitudes y Hadamard combina los caminos que llegan a una
misma etiqueta, incluida la interferencia destructiva. Se eliminan únicamente sumas exactamente cero
en la aritmética numérica; no se aplica un umbral para podar amplitudes pequeñas. Se emplean valores
`double complex`, por lo que subsisten el redondeo y los límites numéricos del simulador.

Medir aplica la regla de Born sobre el estado completo almacenado y normaliza la rama observada.
`reset` sigue siendo una operación explícita de medición y corrección. El modo no reemplaza una
superposición por una muestra clásica para ejecutar las puertas.

También se corrige el corte fijo anterior de medición: el motor denso forzaba probabilidades menores
que `1e-15` a cero. Una reproducción con probabilidad próxima a `1e-16` y semilla determinista
confirma el resultado erróneo anterior y el resultado corregido. Esto no elimina las limitaciones de
precisión de coma flotante o del generador pseudoaleatorio.

## Presupuestos y API

El modo predeterminado sigue siendo `dense`, con un límite de 24 qubits. Para elegir el nuevo modo
en un ejecutable nativo se usa `CKET_SIMULATOR=sparse`. Su presupuesto predeterminado es de 65536
etiquetas ocupadas, configurable entre 1 y 1048576:

```bash
CKET_SIMULATOR=sparse CKET_SPARSE_STATES=4096 build/blocks/02/wide_integers
```

Una Hadamard puede crear hasta dos contribuciones por etiqueta. El motor combina las contribuciones
y comprueba el límite antes de sustituir el estado de esa puerta. Si el soporte resultante supera el
presupuesto, el programa falla con un diagnóstico; no continúa con un estado truncado. Las puertas
anteriores de la operación pueden haber sido ejecutadas, y el contexto fallido no se puede seguir
usando. Los residuos numéricos no nulos pueden consumir soporte aunque el cálculo ideal los
cancelara exactamente.

La API C del motor añade `qc_create_sparse`, `qc_sparse_size` y `qc_sparse_entry`. Esta última
devuelve las partes baja/alta de la etiqueta y su amplitud. `qc_state_size` conserva el significado
de extensión densa y devuelve cero en un contexto disperso; no sirve para enumerarlo. `qc_amplitude`
permite consultar etiquetas representables por `size_t`. La acción `--state` del backend continúa
usando el motor denso; la variable de entorno selecciona el runtime de los ejecutables nativos.

La [evidencia dirigida](audit-evidence/sparse-simulation-v1/README.es.md) identifica las fuentes y
las comprobaciones normales y ASan/UBSan. Este avance no completa el bloque 02 ni C11: siguen
pendientes contextos enteros generales, recursos de circuitos que superen 128 qubits y el resto del
inventario. Una superposición de todos los valores de un entero de 32 bits también supera el
presupuesto disperso. No hay una afirmación de ejecución en QPU o aceleración cuántica.
