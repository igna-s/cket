# Referencia de memoria con guarda de dominio explícita

27 de octubre de 2026. Seguimiento del CI fallido de `0740cfa` y del SDK de entradas comprobadas. La
candidata continúa siendo RC3; la 1.0 estable y la integración completa siguen pendientes.

## Fallo y corrección

La referencia anterior falla con `unmodelled retained version` en el primer relleno de longitud cero
con una iteración. `memset(a, a[0] + 1, 0)` captura su valor de relleno y conserva una guarda
privada de dominio. No escribe el destino. La referencia modelaba la captura pero omitía esa guarda,
cuyo valor esperado es cero porque estas llamadas están dentro del dominio definido.

La corrección asigna explícitamente cero a esa salida privada. No usa certificados de valor conocido
para decidir el resultado esperado, no acepta registros desconocidos y mantiene la comparación de
todas las amplitudes, fases, historia y evolución independiente de puertas. No modifica el productor
ni las reglas de propiedad o no clonación.

## Evidencia

| Comprobación               | Resultado y alcance                                                                                                                                                                 |
| -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Fuentes                    | 240 entradas congeladas; solo cambia `scripts/validate-runtime-memory.py` respecto del SDK de entradas comprobadas.                                                                 |
| Normal O0/O2               | 600 puntos de estado complejo conjunto, 180 inversas completas, seis ejecuciones de frontera y ocho rechazos en compilación.                                                        |
| ASan/UBSan O1              | 300 puntos de estado complejo conjunto, 90 inversas completas, tres ejecuciones de frontera y cuatro rechazos en compilación.                                                       |
| Perfiles                   | Copia, movimientos en ambos sentidos, relleno y bytes parciales; longitudes 0/1/2, iteraciones 0/1/3 y ambas políticas de síntesis.                                                 |
| Referencia anterior        | El mismo estado correcto reproduce su fallo; se conserva también el registro del CI anterior.                                                                                       |
| Contratos de la referencia | Un contexto correcto pasa. Se rechazan tres mutaciones unitarias: valor público, fase relativa y guarda privada. Sus evoluciones e inversas se validan antes del rechazo semántico. |
| Productores                | Fuentes y binarios normales e instrumentados sin cambios; hashes explícitos por componente.                                                                                         |

[summary.json](summary.json) identifica las fuentes, comandos y componentes. `evidence.tar.gz`
conserva la instantánea, trazas, logs, referencias anterior y corregida, contratos de mutación y sus
helpers. Las matrices usan dos etiquetas coherentes por perfil; no prueban todos los estados de un
buffer ni todos los programas C. Las categorías anteriores tienen alcances distintos y no son un
porcentaje de cobertura.

El [SDK](../../../output/sdk/checked-entry/README.es.md) conserva su archivo y validador originales;
la referencia corregida se ejecuta desde el checkout actual contra los mismos binarios. No se
reescribe su manifiesto histórico. El [contrato de memoria](../../c11-runtime-memory.md) explica
vidas, direcciones, historia y recursos pendientes. La integración general, alias/ABI y flotantes
siguen abiertos. No se afirma ejecución en QPU física ni aceleración. La ciberseguridad permanece
clásica.
