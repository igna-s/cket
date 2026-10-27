# Procedimientos con retornos CPU y limpieza léxica

27 de octubre de 2026. Ampliación posterior a RC3. Se implementan retornos `void` anticipados en
bloques, condiciones y bucles CPU, incluidos `for`, `while` y `do while`. Termina la llamada
expandida y continúa el llamador. La [nota](../../c11-procedure-returns.md) describe el contrato y
el [ejemplo](../../../examples/procedure_returns.c) combina visitas, retorno e interferencia.

La expansión usa una bandera CPU local por llamada. Las sentencias, condiciones y pasos posteriores
al retorno se omiten, pero el bloque termina su limpieza léxica. No se mide el control cuántico ni
se elimina historia desconocida. Cada llamada anidada conserva su propia bandera.

## Corrección adicional de ramas

Un programa anterior sin retornos, con `if(n) x ^= true; else x ^= true;` y un `x` inicialmente
conocido, era rechazado. Ambas ramas exportaban certificados de su estado de entrada como si fueran
hechos incondicionales. El verificador estructural recorre ambas ramas y detectaba el certificado
incorrecto al llegar a la segunda. Se reproduce con el compilador publicado y se conserva el fallo.
El frontend deja de exportar esas afirmaciones locales dentro de ramas CPU; el verificador mantiene
sus comprobaciones y la ejecución nativa selecciona una sola rama.

## Resultados y fuentes

| Configuración                 | Resultados dirigidos                                                                                                         |
| ----------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Normal, cinco niveles GCC     | 700 ejecuciones frente a C, 500 puntos de estado complejo, 250 inversas completas y 20 rechazos en compilación.              |
| ASan/UBSan O1                 | 140 ejecuciones frente a C, 100 puntos de estado complejo, 50 inversas completas y cuatro rechazos en compilación.           |
| Regresiones por configuración | 305 préstamos de lectura, 519 bindings, 90 contratos de entrada, 395 pruebas del núcleo, 2332 de historial y 940 de relleno. |
| Programa nuevo                | Cinco límites CPU, incluidas cero visitas y ejecución completa del bucle, en ambas configuraciones.                          |
| Memoria con longitud CPU      | 900 puntos de estado complejo y 270 inversas, normales O0/O2 y ASan/UBSan O1, con estas mismas fuentes.                      |

Las 14 familias nativas comprueban resultados, efectos CPU una sola vez, cortocircuito, pasos de
bucle y llamadas anidadas/repetidas. Las cinco familias de estado usan cuatro etiquetas coherentes,
fases relativas, un espectador y auxiliares que deben quedar limpios. Una de ellas transforma el
estado mediante X y fases antes de retornar. Se comparan todas las amplitudes con fórmulas C y con
evolución independiente de puertas; después se invierte la secuencia completa. Los puntos repetidos
no son cobertura porcentual ni pruebas de todos los estados o programas C.

La regresión de memoria usa sus cinco perfiles de longitudes 0/1/2 e iteraciones 0/1/3, con dos
etiquetas coherentes por perfil. Su alcance es distinto de las cuatro etiquetas de los retornos; los
recuentos no se suman como estados independientes o porcentaje de compatibilidad C.

[summary.json](summary.json) identifica 242 entradas congeladas. Cambian dos archivos de producción
del frontend; driver, runtime, backend y motor conservan sus fuentes y binarios. El plugin se
reconstruye. `evidence.tar.gz` conserva fuentes, comandos, trazas, hashes, regresiones y las
preparaciones anteriores separadas. Una expectativa histórica del núcleo exigía rechazar un retorno
CPU ahora implementado; se sustituye por el rechazo del retorno bajo control cuántico. El recuento
del núcleo sigue siendo 395.

El [SDK nuevo](../../../output/sdk/cpu-returns/README.es.md) conserva las fuentes de esta ampliación
y los paquetes anteriores mantienen sus propias identidades. La integración general, los resultados
propios con retornos múltiples y efectos, el CFG arbitrario, alias/vidas/ABI generales y flotantes
siguen pendientes. No se afirma una 1.0 estable, ejecución en QPU física ni aceleración. La
ciberseguridad permanece clásica.

Se conserva además una llamada rechazada dentro de un `if` cuántico. Componer la bandera privada CPU
con un llamador coherente sigue pendiente; no se clasifica como imposibilidad física.
