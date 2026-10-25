# Hallazgo: pérdida de coherencia en sobrescrituras condicionadas

**Estado actual:** el seguimiento de 1.0.0-rc.2 al final de este informe corrige los perfiles
publicados del heap y registra un nuevo contraejemplo en el byte intacto de una palabra fija. Los
apartados anteriores conservan las fuentes y resultados de cada revisión.

Revisión del 24 de octubre de 2026 sobre el compilador de la entrega `da2ffb31e568`, publicada con
su demo y nota técnica en `2fe516fba203`.

## Contraejemplo reproducido

Con fuente y destino en superposiciones independientes, una sobrescritura fija bajo `if(control)`
calcula un destino nuevo mediante `select(control, source, previous_destination)` y conserva la
versión anterior. En la rama inactiva eso puede calcular dos registros correlacionados del valor
anterior, cuando correspondía conservar su estado sin esa nueva correlación.

La prueba prepara `y = (|0> + exp(0.73i)|1>)/sqrt(2)` y usa un control cero. Tanto
`if(control) memcpy(&y, &x, n)` con `n = 1` como `if(control) y = x ^ 0` reducen la pureza local de
`y` de 1 a 0.5. El error máximo entre las matrices de densidad es 0.5. Los contadores de medición y
reset permanecen en cero: la pérdida local de coherencia proviene de entrelazamiento innecesario con
historia, no de un canal explícito ni de clonación física independiente.

También se reproduce el error al proyectar sobre `control = 0` un control originalmente en
superposición. Comprobar únicamente los valores de base, la unitariedad de las puertas o su inversa
completa no detectaba este cambio de semántica de la rama inactiva.

## Seguimiento del 25 de octubre: estado y alcance

**La corrección es parcial; quedan fallos cuánticos abiertos.** El cambio de buffers fijos calcula
el valor activo en un auxiliar cero y usa un intercambio controlado. La rama inactiva conserva el
destino; la historia recibe el valor anterior solo donde se escribe. Los cuatro contraejemplos
directos dejan de perder pureza. Las expresiones guardadas de GCC conservan una vista del valor
anterior para que un postfix no lea el valor actualizado.

La regresión nueva compara 240 estados conjuntos completos y 120 inversas en cinco perfiles, con
ambas políticas de síntesis, GCC O0/O2 normales y O1 instrumentado. Su alcance incluye préstamos que
siguen vivos en el punto observado; las salidas de bloque y el heap conservan pendientes.

La ampliación inicial de la revisión encontró estos resultados sobre la corrección de buffers fijos.
Las dos identidades del heap se corrigen en el seguimiento posterior de este informe:

| Contexto                                                                                        | Resultado cuántico observado                                    | Estado                                                                             |
| ----------------------------------------------------------------------------------------------- | --------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| Copia y asignación fija directas, rama inactiva                                                 | Pureza 1 → 1                                                    | Corregidas en los perfiles finitos publicados.                                     |
| Un préstamo temporal no observado sale de su bloque después de una asignación bajo control cero | Pureza del dueño 1 → 0.5                                        | Corregido en los perfiles finitos de inversas condicionadas del seguimiento final. |
| Copia coherente sobre heap, sector inactivo                                                     | Pureza conjunta de la fuente de referencia y el destino 1 → 0.5 | Abierto: el estado local de cada registro por separado ocultaba el fallo.          |
| `p[0] = p[0]` sobre un heap entrelazado                                                         | Pureza conjunta 1 → 0.5                                         | Corregida en el seguimiento de identidades; pureza actual 1 → 1.                   |
| `memset(p, p[0], 1)` sobre ese heap                                                             | Pureza conjunta 1 → 0.5                                         | Corregido en el seguimiento de identidades; pureza actual 1 → 1.                   |
| `memmove(p, p, 1)` sobre ese heap                                                               | Pureza conjunta 1 → 1                                           | Identidad conservada en el caso comprobado.                                        |

Todos esos contrastes se observan antes de medir o resetear. Una secuencia de puertas unitarias y su
inversa pueden ser correctas mientras la traducción sigue equivocándose al retener información
redundante o al limpiar un préstamo. Por eso no basta contar valores medidos ni puertas invertidas.

La [cápsula de progreso](audit-evidence/coherent-store-progress-v1/README.es.md) separa la regresión
que pasa de los contraejemplos que estaban abiertos en esa revisión. El seguimiento siguiente cierra
las dos identidades del heap; las inversas que necesitan una vista condicionada del dueño anterior y
las copias coherentes no idénticas siguen pendientes. Conservar indefinidamente un auxiliar no
sustituye esa limpieza.

## Seguimiento del 25 de octubre: identidades del heap

`p[0]=p[0]` y el relleno idéntico `memset(p,p[0],1)` conservan ahora pureza conjunta 1 → 1 en los
contraejemplos anteriores. El motor demuestra la igualdad de bits con la palabra existente antes de
crear un dueño nuevo. Solo expande operaciones puras acotadas, respeta las guardas y épocas de cada
definición y verifica operandos descartados. Si no demuestra la identidad, sigue la síntesis
ordinaria. La prueba no lee amplitudes ni introduce canales.

La [cápsula de identidades](audit-evidence/heap-identities-v1/README.es.md) congela 225 entradas y
registra 2232 puntos de estado completo y 1116 inversas: ocho tipos enteros de 8 a 64 bits, ocho
perfiles, 0/1/8 iteraciones, ambas políticas de síntesis y construcciones normales O0/O2 e
instrumentada O1. Se comprueban fases, dueños y cables estables y auxiliares cero. Dos perfiles de
captura devuelta de 64 bits exceden 128 cables y se excluyen explícitamente; las capturas se
comprueban hasta 32 bits. El relleno nativo es de un byte; máscaras altas y ambos órdenes de bytes
tienen evidencia adicional del motor, con su alcance separado.

También se corrige la captura única de argumentos CPU de `memset` sobre heap, incluso con longitud
cero. Los efectos CPU dentro de control cuántico o mezclados con una carga cuántica siguen
rechazados. Las nuevas 1682 aserciones del motor y las regresiones afectadas pasan en ambas
construcciones. En esa revisión quedaban el préstamo bajo control cero y el sector inactivo de copia
coherente no idéntica al heap, con pureza 1 → 0.5. El seguimiento posterior corrige el primero y
reproduce una variante con control en superposición; la corrección sigue siendo parcial.

La demo `buffer_lengths.c` publicada no contiene esta combinación: sus escrituras de memoria no
están bajo control cuántico y sus destinos iniciales son conocidos. Las campañas anteriores
conservan sus resultados finitos, pero sus recuentos no certifican este contraejemplo ni todo el
compilador.

Una revisión adicional compara cinco perfiles con amplitudes complejas no uniformes, destinos
independientes, copia, movimiento de propiedad, `free` y reducción con `realloc`. Sus 30 estados
completos y 10 inversas pasan en la configuración exploratoria O2 sin tablas. Ese resultado no
subsana el fallo de las ramas inactivas; tampoco certifica todos los contextos del heap.

La cápsula
[coherent-overwrite-coherence-v1](audit-evidence/coherent-overwrite-coherence-v1/README.es.md)
conserva los programas, logs, matrices y herramientas usadas. La referencia física de no clonación
es [Wootters y Zurek, Nature 299, 802–803 (1982)](https://doi.org/10.1038/299802a0).

La ciberseguridad permanece clásica y sin cambios.

## Seguimiento del 25 de octubre: limpieza ante inactividad demostrada

El préstamo temporal del contraejemplo original bajo `control=0` conserva ahora su definición
invertible. `qmaterialize` recibe las guardas de la escritura y solo elimina la definición si no
puede demostrar inactividad global. Un intercambio inactivo tampoco consume préstamos ni cambia
épocas. La limpieza al salir del bloque invierte realmente el temporal: pureza 1 → 1 y fase relativa
esperada, con cero mediciones/resets.

La [cápsula de préstamos inactivos](audit-evidence/inactive-borrows-v1/README.es.md) congela 227
entradas y registra 252 puntos de estado completo y 126 inversas, O0/O2 normales y O1 ASan/UBSan,
con ambas políticas. Incluye asignaciones bool/8/16/32 bits y copias de un byte hacia palabras de
8/16/32 bits; guardas cero, uno negado y cero preparado desde CPU. Una primera copia de palabra
completa de 32 bits excedió 128 cables; la limpieza de préstamos de 64 bits tampoco se califica. Las
139 aserciones nuevas del motor comprueban certificados, épocas obsoletas y rechazos antes de mutar.
Las regresiones de identidades y sobrescrituras se repiten con esta revisión y conservan sus
recuentos separados.

**Persisten dos contraejemplos bajo control en superposición.** En la variante del préstamo,
proyectar y normalizar sobre el `control=0` original da pureza 1 → 0.5 después de salir del bloque.
La copia coherente no idéntica al heap conserva su fallo de pureza conjunta sectorial 1 → 0.5. Los
programas y estados crudos están archivados. La inversa general debe reconstruir el valor anterior
desde su ubicación condicionada; demostrar que todo el circuito es unitario no resuelve esa
obligación. La ciberseguridad permanece clásica.

## Candidata 1.0.0-rc.1: inversas desde el dueño anterior condicionado

La [cápsula de inversas condicionadas](audit-evidence/rebound-borrows-v1/README.es.md) congela 231
entradas. Los dos contraejemplos de préstamos, con control cero y con control en superposición,
conservan ahora pureza 1 → 1 y el efecto de fase. El intercambio transporta las recetas de los
préstamos mediante vistas del valor anterior; no materializa todas las dependencias como raíces. Las
decisiones privadas reconstruibles desde raíces estables se expanden en la receta para limpiar sus
copias físicas. Se verifica el estado antes, durante y después del préstamo.

Los 1368 puntos de estado y 456 inversas incluyen bool/8/16/32 bits, negativos -2/-1, controles
abiertos/anidados, copias y rellenos de un byte, préstamos encadenados/movidos/aritméticos y función
del mismo archivo. Pasan O0/O2 normales y O1 ASan/UBSan, con ambas políticas. Las 34197 aserciones
del motor tienen su alcance separado de las pruebas del frontend. Los prototipos fallidos se
archivan aparte: control privado redundante, certificado negativo incorrecto y exceso de espacio
para reconstruir un booleano. La [nota técnica](paper/conditional_inverse_note.es.md) detalla la
identidad y las precondiciones.

**Sigue abierto el contraejemplo del heap:** copia no idéntica bajo control en superposición, sector
inactivo con pureza 1 → 0.5 y error de densidad 0.5, sin mediciones ni resets. Los casos directos
fijos y el heap con control cero del contraste conservan error cero. No se afirma que este sea el
único defecto restante: las vidas generales, escrituras repetidas, destinos derivados y rangos
correlacionados requieren contratos y evidencia adicionales. La candidata no cierra el perfil
cotidiano ni recalifica los papers anteriores. La ciberseguridad permanece clásica.

## Candidata 1.0.0-rc.2: heap y bits que no se escriben

El contraejemplo de copia coherente no idéntica al heap conserva ahora pureza conjunta sectorial 1 →
1, con error cero. El destino mantiene sus cables y el intercambio controlado conserva el valor
anterior solamente donde se escribe. Se amplía el intercambio a una máscara de bits: los bytes
intactos del heap conservan su coherencia también en la rama activa. Las fuentes de `memmove`
solapado consultan las recetas de sus versiones originales. Las escrituras con índice cuántico usan
un predicado por elemento y no duplican los elementos no seleccionados.

La [cápsula](audit-evidence/heap-coherence-v1/README.es.md) identifica 233 entradas y conserva 2670
comparaciones de densidad pública con fórmulas explícitas de historia, 2670 estados conjuntos
contrastados mediante otro intérprete de puertas y 852 inversiones completas. Son alcances
distintos. Pasan O0/O2 normales y O1 ASan/UBSan, ambas políticas y 97 programas por nivel, incluidos
préstamos de 32 bits, negativos, alias de relleno/copia, movimientos e índices cuánticos en sus
perfiles y límites. La [nota](paper/heap_coherence_note.es.md) explica las identidades.

**Nuevo contraejemplo abierto en buffers fijos:** preparar una palabra con byte alto en
superposición y copiar solamente el byte bajo bajo control cuántico puede retener una copia del byte
alto. Proyectando sobre el control activo, la pureza conjunta del byte alto y su referencia cae de 1
a 0.5; el error de densidad es 0.5. No hay mediciones ni resets antes de observarlo. El mismo
contraste en el heap conserva pureza 1 → 1 en ambas ramas con la nueva máscara. Los programas,
estados y herramientas de este contraste están en la cápsula actual. La ruta de buffers fijos
todavía no emite ese intercambio parcial. No se afirma que sea el único defecto restante; la
auditoría general y las vidas/alias/ABI siguen abiertos.

El arnés inicial seleccionó el simulador disperso también para el núcleo, que esperaba un rechazo de
32 cables del modo denso. Se conserva ese 394/1 y la continuación con simulador predeterminado,
395/0 en ambas construcciones, junto con las matrices que ya habían terminado. No se cuenta el fallo
de configuración como defecto del compilador ni se oculta en una campaña ficticiamente
ininterrumpida. Los papers históricos y el CI remoto mantienen sus revisiones originales.
