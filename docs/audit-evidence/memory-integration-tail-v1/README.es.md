# Continuación de memoria tras el gate de longitudes runtime

27 de octubre de 2026. Se ejecutan las dos matrices que siguen a la referencia de memoria runtime en
`scripts/verify.sh`. La [corrección de esa referencia](../runtime-memory-reference-v1/README.es.md)
conserva su evidencia separada. No se repiten aquí todos los gates anteriores ni se declara la
integración completa aprobada.

| Matriz                     | Normal O0/O2                                          | ASan/UBSan O1                      |
| -------------------------- | ----------------------------------------------------- | ---------------------------------- |
| Escrituras coherentes      | 160 puntos de estado complejo, 80 inversas completas. | 80 puntos, 40 inversas completas.  |
| Memoria fija en bucles CPU | 200 puntos de estado complejo, 60 inversas completas. | 100 puntos, 30 inversas completas. |

Ambas políticas de síntesis pasan. La primera matriz cubre cinco perfiles de escritura, ramas
inactivas/coherentes, valores anteriores, préstamos y una condición cuyo origen cambia. La segunda
cubre copia, movimientos en ambos sentidos, relleno y bytes parciales con 0/1/3 iteraciones.
Comparan estados complejos con fórmulas C y evolución independiente de puertas e invierten la
secuencia completa. Son programas y estados finitos; no califican todos los estados ni todos los
flujos de memoria.

[summary.json](summary.json) identifica las mismas 240 entradas de la corrección de referencia. Las
fuentes y los binarios de producción permanecen iguales a los del SDK de entradas comprobadas.
`evidence.tar.gz` conserva la instantánea, comandos, trazas, logs y hashes de los componentes. Los
recuentos de estas matrices se solapan con los de sus campañas históricas; no son nueva cobertura
porcentual ni una prueba de toda la biblioteca.

La 1.0 estable, vidas/alias/ABI generales, flotantes y la integración completa siguen pendientes. No
se afirma QPU física ni aceleración. La ciberseguridad permanece clásica.
