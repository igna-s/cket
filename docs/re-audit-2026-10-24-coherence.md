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

## Estado y alcance

**El hallazgo está abierto.** Las sobrescrituras condicionadas de destinos cuánticos desconocidos
necesitan una implementación que transporte la información anterior solo en la rama que escribe. Las
dependencias de inversas y los alias deben conservar su significado después de ese transporte. No
debe cerrarse el caso únicamente porque vuelva a pasar una comparación de valores medidos.

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
