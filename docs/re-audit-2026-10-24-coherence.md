# Hallazgo: pérdida de coherencia en sobrescrituras condicionadas

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

La ampliación de la revisión encontró estos resultados sobre el compilador corregido:

| Contexto                                                                                        | Resultado cuántico observado                                    | Estado                                                                    |
| ----------------------------------------------------------------------------------------------- | --------------------------------------------------------------- | ------------------------------------------------------------------------- |
| Copia y asignación fija directas, rama inactiva                                                 | Pureza 1 → 1                                                    | Corregidas en los perfiles finitos publicados.                            |
| Un préstamo temporal no observado sale de su bloque después de una asignación bajo control cero | Pureza del dueño 1 → 0.5                                        | Abierto: conservar el temporal como raíz impide limpiar su correlación.   |
| Copia coherente sobre heap, sector inactivo                                                     | Pureza conjunta de la fuente de referencia y el destino 1 → 0.5 | Abierto: el estado local de cada registro por separado ocultaba el fallo. |
| `p[0] = p[0]` sobre un heap entrelazado                                                         | Pureza conjunta 1 → 0.5                                         | Abierto: la autoasignación crea historia redundante.                      |
| `memset(p, p[0], 1)` sobre ese heap                                                             | Pureza conjunta 1 → 0.5                                         | Abierto: el autorrelleno de un byte no conserva la identidad.             |
| `memmove(p, p, 1)` sobre ese heap                                                               | Pureza conjunta 1 → 1                                           | Identidad conservada en el caso comprobado.                               |

Todos esos contrastes se observan antes de medir o resetear. Una secuencia de puertas unitarias y su
inversa pueden ser correctas mientras la traducción sigue equivocándose al retener información
redundante o al limpiar un préstamo. Por eso no basta contar valores medidos ni puertas invertidas.

La [cápsula de progreso](audit-evidence/coherent-store-progress-v1/README.es.md) separa la regresión
que pasa de los contraejemplos que siguen abiertos. El siguiente trabajo debe resolver las
identidades del heap y las inversas que necesitan una vista condicionada del dueño anterior;
conservar indefinidamente un auxiliar no sustituye esa limpieza.

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
