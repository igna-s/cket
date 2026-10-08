# Tipos, propiedad y efectos de `|c>`

## C real y calificacion cuantica

GCC procesa C11 completo, incluidos preprocesador, headers, typedefs y las promociones aritmeticas. El plugin registra `cket_quant` y los intrinsecos, y analiza los arboles en `PLUGIN_PRE_GENERICIZE`. `quant`, `cuant` y `Cuant` son macros del header `quantum.h` para ese atributo reconocido. No son palabras reservadas incorporadas aun a la gramatica de GCC.

```text
SemanticType = { GCC_C_Type, quantum, integer_width, signedness }
```

`int` y `quant int` conservan la misma anchura y codificacion C, pero tienen semanticas diferentes. La calificacion vive en el analisis del plugin y en la tabla de tipos de la Quantum IR. No se presenta el atributo como si fuera un calificador nativo de GCC. Una futura modificacion del frontend debe propagar la calificacion a tipos compuestos, argumentos, retornos, typedefs y ABI.

Se admiten enteros C y bool de hasta 64 bits, arrays fijos multidimensionales y estructuras enteras. Las funciones se componen sobre el AST de GCC: parametros escalares, punteros y arrays prestan registros, los resultados pueden ser expresiones puras condicionales o locales propios con retorno terminal, y los procedimientos pueden crear temporales propios. Sus definiciones deben preceder las llamadas en la misma unidad de traduccion. Los propietarios pertenecen a `main`, incluidas las variables de procedimientos expandidos. Almacenamiento global, VLA, unions, bitfields, coma flotante y una ABI cuantica general siguen pendientes. Una marca interna volatile mantiene los accesos cuanticos ante simplificaciones como `x*0`.

`quant uint8_t *p = &x` es una referencia con direccion clasica al registro existente. Se resuelven dereferencias y desplazamientos constantes dentro de limites comprobados. Se admiten alias locales, pero no escape a memoria CPU ni reasignacion dinamica. Los parametros mutables no pueden solaparse; un prestamo de solo lectura no puede reenviarse a un mutador. Los prestamos no pueden consumirse mediante movimientos o `find`, incluso a traves de alias. Los procedimientos admiten un `return;` final; los retornos tempranos necesitan analisis adicional.

Arrays escalares fijos y sus prestamos admiten `a[i]` con indice CPU en suma/resta y XOR de enteros sin signo con operandos CPU, incrementos, `measure`, `reset` y Fourier/inversa. El indice se evalua una vez y se comprueba contra los limites; no mide el registro para seleccionarlo. Estas operaciones pueden ejecutarse en bucles CPU sin asignar nuevos registros. Medir/resetear un elemento desconocido no demuestra limpieza o consumo de todos los propietarios. El acceso coherente con indice cuantico y las expresiones cuanticas generales sobre elementos seleccionados siguen pendientes.

Una inicializacion desde CPU, como `quant uint8_t x = read_value();`, evalua el valor una vez, conserva su conversion C y prepara un estado de base nuevo. No lee otro registro sin medirlo explicitamente. Los productos tambien pueden tener inicializadores CPU. La preparacion dinamica necesita ejecucion nativa y no se aplana para QASM.

## Valores nuevos y asignacion

```c
quant uint8_t x = superposition(0, 15);
quant uint8_t y = x % 2;
```

Genera almacenamiento cero y una operacion reversible:

```text
|x>|0> -> |x>|x % 2>
```

Esto conserva informacion y puede entrelazar registros; no clona un estado arbitrario. Los operadores se serializan con su tipo GCC en una expresion IR. Por ejemplo, `x*x` con x de tipo uint8_t conserva la promocion de C a int y no compara silenciosamente un producto truncado a 8 bits. Solo la conversion al resultado uint8_t trunca a esa anchura.

Una inicializacion directa `quant uint8_t b = a` con tipo compatible es una **transferencia de propiedad**: los dos nombres no tienen acceso simultaneo a dos estados. `a` queda consumido y sus usos posteriores producen un error. El movimiento y el resultado de `find` requieren el mismo tipo C canonico: typedefs compatibles se admiten, pero la misma anchura y signo no bastan entre tipos C distintos. Un cambio de anchura o signo calcula una conversion de valor y conserva la entrada.

Las actualizaciones `x += k` y `x -= k` sobre tipos sin signo usan suma modular reversible. La suma/resta entre registros sin signo distintos conserva la fuente y tiene sintesis directa sin tablas ni ancillas. `x ^= y` y XOR con operandos independientes admitidos conservan informacion; los operandos CPU se evaluan una vez. Otras actualizaciones se sintetizan como permutaciones solo si son inyectivas en todo el espacio de bits. `x = x % 2`, desplazamientos destructivos y overwrites de un registro desconocido se rechazan. Actualizar un resultado derivado requiere otro resultado nuevo.

Un operando CPU con efectos, como `x += measure(y)` o `x += ++n`, se evalua una sola vez antes de actualizar x. El plugin conserva el `SAVE_EXPR` compartido generado por GCC y emite la medicion explicita antes de la suma en Quantum IR. Esos efectos CPU se rechazan dentro de control cuantico coherente.

## Control de flujo y efectos

Una condicion quantum en un `if/else` se calcula en un auxiliar, controla coherentemente las operaciones de ambas ramas y se descomputa. No se elige una rama mediante medida. Las ramas aceptan operaciones unitarias soportadas y `phase_flip()`, y rechazan I/O, llamadas clasicas, escrituras CPU, reset, medicion y saltos clasicos. Las condiciones anidadas tienen controles acumulados.

Modificar una entrada de la condicion puede hacer imposible su inversa. El compilador rechaza ese caso mientras no tenga una transformacion reversible demostrada. Un flag independiente puede controlar una actualizacion de otro registro.

En particular, `if (x == 0) x += 1;` manda tanto cero como uno a uno. Es una transformacion no inyectiva sobre x, no solamente una limitacion conservadora del analisis. Conservar un historial distingue las entradas, pero no garantiza poder limpiarlo manteniendo ese resultado.

Los `for/while/do` clasicos conservan el control CPU real de GCC. Las actualizaciones sin signo pueden recibir operandos CPU en cada iteracion. Las operaciones indexadas admitidas tambien pueden medir/resetear elementos de almacenamiento existente, conservando obligaciones de vida para todos los propietarios. Las declaraciones cuanticas dentro de flujo CPU dinamico siguen requiriendo analisis adicional. Se admiten medidas/reset escalares e if coherentes con propietarios estables; las uniones conservan solo hechos ciertos en todos los caminos, incluido el de cero iteraciones de un bucle. Los bucles con condiciones cuanticas requieren una cota finita y una representacion reversible del historial; no se admiten bucles generales sin esa prueba.

Los `for` con contador local, limites constantes y paso `++`/`--` pueden expandirse hasta 4096 iteraciones si permiten productos, temporales o control coherente. Se comprueban limites y desbordamiento; no se admiten saltos ni cambios del contador en el cuerpo. Los bucles CPU restantes conservan su ejecucion, incluida la falta de terminacion de un contador que vuelve a cero. La Quantum IR marca el control CPU dinamico y rechaza aplanarlo para QASM.

## Fases y transformada de Fourier

`phase(theta)` aplica un angulo clasico finito, constante o calculado en CPU, a los controles activos. Fuera de control es una fase global. Los operandos CPU con efectos se rechazan bajo control coherente. Un angulo dinamico requiere ejecucion nativa.

`fourier(x)` implementa `|x> -> sum(k, exp(2*pi*i*x*k/2^w)|k>)/sqrt(2^w)` sobre todo el registro de anchura w, en orden little-endian. `inverse_fourier(x)` cambia el signo del exponente. Admiten control coherente, conservan correlaciones y tienen sintesis cuadratica sin tablas. Una transformacion de base invalida las pruebas de valor y uniformidad, y se rechaza si impide una inversa pendiente.

Los ejemplos de Fourier y estimacion de fase comparan amplitudes con una DFT matematica independiente. Se utiliza precision double y tolerancia numerica `1e-10` en las pruebas; no se declara aun un presupuesto general de error para cualquier algoritmo.

## Descomputacion y medicion

Cada valor calculado registra su expresion creadora, controles externos normalizados y versiones de entradas y controles. Tras su ultimo uso, el compilador aplica la inversa y libera el registro solo cuando esta probado cero. La eliminacion sigue el orden inverso de dependencias. El backend exige la misma expresion, mascara y patron de control y dependencias disponibles con las mismas versiones; rechaza modificar, medir, resetear o liberar dependencias necesarias para una limpieza pendiente.

Una fase diagonal puede conservar esa prueba porque no cambia los valores de base utilizados por el calculo. Sus fases relativas sobreviven a la inversa. La prueba de uniformidad para find se invalida cuando corresponde. El certificado actual compara el texto JSON de la expresion, por lo que puede rechazar serializaciones equivalentes.

```c
quant bool flag = (x + 5 == 42);
if (flag) phase_flip();
// flag y el auxiliar de control vuelven a cero automaticamente.
```

La sintesis finita actual compone directamente el oraculo booleano reversible de la expresion IR; no necesita almacenar cada nodo aritmetico por separado. Los registros temporales existentes si se limpian explicitamente en IR mediante `quncompute` y `qrelease`.

`measure(x)` es la unica conversion a un valor CPU. Colapsa explicitamente el registro y conserva el estado resultante para siguientes operaciones/mediciones. `reset(x)` permite destruccion explicita de informacion y deja almacenamiento cero. No hay mediciones o resets insertados para facilitar el control coherente o la limpieza. Las constantes, la preparacion y las puertas unitarias nunca sobrescriben un estado desconocido mediante inicializacion no unitaria.

Antes de salir, todo registro debe estar medido/reset explicitamente, transferido a otro propietario o descomputado y liberado. Se rechazan retornos con vida cuantica que no se pueda demostrar segura. El runtime C ejecuta cada operacion inmediatamente en su motor enlazado, incluida la limpieza; al salir libera la memoria del simulador.

## Motor nativo en C

El vector de estado contiene amplitudes `double complex` en orden little-endian. H transforma un par de amplitudes `(a,b)` en `((a+b)/sqrt(2),(a-b)/sqrt(2))`; X y sus controles permutan amplitudes, y una fase multiplica las amplitudes seleccionadas por `cos(theta)+I*sin(theta)`. Las operaciones unitarias conservan la superposicion sin muestrearla.

Solo `measure` toma muestras segun la regla de Born, elimina las amplitudes incompatibles y normaliza el estado restante. Una medicion parcial conserva correlaciones con los demas registros. Un `reset` explicito aplica el canal de reset mediante una trayectoria de colapso y correccion, permitido porque el programa solicito destruir informacion. Liberar un auxiliar tambien verifica que su probabilidad fuera de cero sea menor que la tolerancia numerica.

El driver `src/cket.c`, el runtime, el parser de Quantum IR, la sintesis, el simulador y las pruebas son C; el plugin AST es C++ por la API de GCC. No se necesita Python ni un SDK. `backend/cket_backend.c` ofrece validacion, QASM, recursos e inspeccion `--state` de amplitudes antes de las mediciones del documento, exclusivamente para depuracion del simulador. Esa inspeccion no modifica la ejecucion de los programas compilados.

## Sintesis y find experimental

El backend intenta sintesis simbolica de operadores de bits, conversiones, desplazamientos constantes y predicados admitidos contra constantes. No enumera el dominio y limita cada bit a 256 terminos, con un presupuesto global de dos millones de pasos. Para aritmetica mas grande hay circuitos por registros; el camino finito optativo usa hasta 12 bits relevantes (forma normal algebraica o minterminos). `--no-tables` lo desactiva. Las permutaciones generales conservan su limite de 12 bits. Los metadatos known y active requieren una prueba independiente mantenida por el motor. La enumeracion construye un circuito; no sustituye su interferencia por respuestas precalculadas.

La expresion debe estar definida en el dominio sintetizado: division por cero, overflow con signo y shifts invalidos detectados en la IR tipada se rechazan. No se demuestra la ausencia de todo comportamiento indefinido que GCC haya simplificado previamente ni en C arbitrario. `&&`, `||` y `?:` puros conservan evaluacion condicional al sintetizar su funcion.

`find(x, predicado)` exige x uniforme e intacto y promete una solucion. `find_known(x,p,M)` recibe una cantidad conocida como promesa del llamador, sin comprobarla mediante enumeracion. Ambos transfieren el registro amplificado y dejan el nombre anterior consumido.

`search(x,p,attempts)` admite cantidad desconocida, incluso cero. Es una operacion explicitamente no unitaria: mide candidatos, verifica p y repite preparacion/Grover con un presupuesto constante entre 1 y 4096. Devuelve bool CPU y conserva x con el ultimo candidato medido. True implica candidato valido; false significa presupuesto agotado, no prueba de inexistencia. Bajo aleatoriedad uniforme independiente, si existe solucion el fallo es a lo sumo `(7/8)^attempts`. Se usa un PRNG reproducible sin sesgo modular; no es una garantia por semilla. El feedback CPU impide una exportacion QASM plana.

El registro auxiliar del oraculo acaba en cero. Los resultados de find son probabilisticos, incluso sin ruido. Los recursos informan qubits logicos, auxiliares, qubits fisicos, puertas y profundidad antes de descomposicion para un dispositivo. Los controles de una y dos entradas se cuentan como CNOT y Toffoli; los controles mayores como MCX, sin inventar su descomposicion. Los controles abiertos son una sola operacion en la IR; su expansion en QASM agrega X de apertura/cierre y puede aumentar el conteo y la profundidad exportados.

## Verificacion y desarrollo hacia 1.0

El verificador revisa esquema, tipos, aridades, controles, conversiones, constantes y angulos finitos antes de sintetizar cada operacion. Revisa estructuralmente todas las ramas de expresiones, conservando el cortocircuito al evaluar su aritmetica. Las marcas CPU deben estar equilibradas, pero no son aun un grafo de control completo.

El [contrato de la IR](ir-verification.md) describe sus pruebas y limites. Ya existen funciones, productos fijos, prestamos, fases, QFT y un ejemplo de estimacion de fase. La [matriz de cobertura de C11](c-coverage.md) enumera lo que falta para el objetivo de extender todo C. El [plan de 1.0](v1-plan.md) mantiene criterios adicionales sobre ABI, sintesis, flujo y precision.

## Referencias

- [GCC: API de plugins y PLUGIN_PRE_GENERICIZE](https://gcc.gnu.org/onlinedocs/gccint/Plugin-API.html).
- [OpenQASM 3: controles y fase global](https://openqasm.com/versions/3.0/language/gates.html).

## Incremento de sintesis, funciones y ejecucion

Hay circuitos por registros para multiplicacion, division/resto con divisor probado distinto de cero y comparaciones generales, con calculo/copia XOR/inversa para limpiar el trabajo. Los operandos negativos conservan truncamiento hacia cero y signo del resto; se prueba ausencia de overflow y se rechaza MIN/-1. Los rangos comprimen extensiones de signo sin cambiar el valor. El divisor de 64 bits necesita trabajo no admitido todavia. Las rutas de tablas pequenas siguen disponibles; `cket_backend ... --no-tables` las desactiva. Anchura del lenguaje (64 bits), limite de puertas (128 qubits fisicos) y simulacion (24) son distintos.

Una funcion con retorno terminal escalar propio puede transferir su local a un inicializador quant nuevo. Las ramas puras de retorno se convierten en una expresion condicional; retornos tempranos con vida pendiente y productos retornados siguen fuera. Los modulos QIR unitarios independientes se componen mediante `circuit` e `inverse_circuit`; [circuit-abi.md](circuit-abi.md) fija su contrato.

El runtime conserva documentos parseados y planes para expresiones, sumas y modulos. Se verifica disponibilidad y versiones cada llamada, y solo se reutiliza trabajo privado libre. La primera llamada todavia puede sintetizar: no se anuncia compilacion anticipada general. El [contrato de 1.0](v1-plan.md) conserva los requisitos no cerrados.
