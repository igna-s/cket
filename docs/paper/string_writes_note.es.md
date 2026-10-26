# Escrituras de cadenas y coherencia de los bytes intactos

26 de octubre de 2026. Bloque de desarrollo posterior a `1.0.0-rc.3`.

`strcpy`, `strncpy`, `strcat` y `strncat` conservan los bytes que la llamada no escribe. En un dato
cuántico esto exige conservar también sus coherencias y correlaciones con otros registros. Comparar
únicamente los números medidos puede ocultar un error de este contrato.

## Defecto reproducido y corrección

La revisión RC3 todavía podía guardar una palabra anterior completa al escribir una parte de ella
mediante un adaptador de cadenas. En los casos reproducidos de `strncpy` de un byte y de `strcpy`
con fuente vacía, la pureza conjunta del byte alto y su referencia bajaba de 1 a 0,5. El error
máximo de densidad era 0,5. No intervenían mediciones ni resets: el historial innecesario retenía
información de un byte que debía permanecer intacto.

Ahora cada actualización usa una máscara efectiva `M` de los bits realmente escritos. Para
`strncpy`, incluye los `n` bytes de destino, también su relleno con ceros. Para `strcpy`, incluye
los bytes hasta el primer NUL, incluido ese NUL. La posición del NUL puede permanecer cuántica. Las
concatenaciones seleccionan su prefijo añadido y su terminador; añadir una cadena vacía es una
identidad sobre los datos, después de validar el destino.

El frontend emite `qpatch` con una máscara clásica de bits potenciales y, cuando hace falta, un
`mask_register` cuántico privado del mismo ancho. Este último contiene las selecciones por rama; no
se obtiene midiendo ni consultando amplitudes del simulador.

## Estado extendido e historial

Sean `Y` la palabra original y `F` el valor calculado de los bytes entrantes. Se calcula `F & M` en
un registro limpio y se intercambian los bits seleccionados por `M`. El resultado es:

```text
destino:  (Y & ~M) | (F & M)
historia: Y & M
```

Estas fórmulas describen valores de base dentro de un circuito reversible extendido. El estado
conjunto conserva sus amplitudes y fases; no son una instrucción para duplicar un estado
desconocido. Una raíz desconocida conserva la información sobrescrita como salida histórica.

Si la máscara depende del destino desconocido, también puede necesitar conservar la decisión
anterior. Si el destino tiene un productor reversible vigente, sus recetas y las capturas
compartidas se transportan hacia las entradas que permanecen estables. Así pueden limpiarse la
máscara y la historia cuando terminan sus consumidores. Las épocas de las dependencias se comprueban
antes de utilizar estas recetas.

Los préstamos del valor anterior reconstruyen la vista con la misma máscara efectiva. Las fases
aplicadas a esas vistas deben corresponder al valor anterior, incluso después de la escritura. Un
resultado terminado cuyo valor de base está certificado puede limpiarse por ese valor; la receta de
una inversa explícita se conserva hasta entonces.

## Guardas y dominios

El análisis conserva certificados de bits cero a través de XOR y de cálculos puros. También reconoce
los valores booleanos de las comparaciones y operaciones lógicas de C aunque su tipo sea `int`.
Estos certificados permiten descartar ramas inalcanzables sin enumerar estados. El bit bajo de un
entero general no determina su verdad: hay una regresión específica para impedir esa confusión.

La precondición completa de una llamada con máscara cuántica se comprueba antes de escribir el
primer destino. Las pruebas de NUL inválido recibido desde la CPU verifican que el estado público
permanezca igual al fallar; una guarda cuántica demostrada inactiva evita evaluar ese dominio. El
diagnóstico todavía puede mencionar la división interna usada como guarda o la ausencia de síntesis
sin tablas. Un mensaje específico de cadenas sigue pendiente.

## Alcance de la evidencia

El validador mantenido es
[`scripts/validate-string-writes.py`](../../scripts/validate-string-writes.py). Compara la densidad
pública con una fórmula independiente, interpreta por separado las puertas emitidas y revierte todas
esas puertas. Ninguna de estas tres comprobaciones sustituye a las otras.

Las matrices congeladas normales O0/O2 y ASan/UBSan O1 completan 1776 comparaciones de densidad
pública, 1776 comparaciones de estado conjunto por otro intérprete y 888 inversas completas. Las 24
comprobaciones de dominios en ejecución se cuentan aparte. La
[cápsula de evidencia](../audit-evidence/string-writes-v1/README.es.md) identifica las fuentes, los
resultados, las tentativas y los fallos generales que permanecen abiertos.

La matriz incluye controles abiertos y anidados, ramas CPU ejecutadas y no ejecutadas, terminadores
cuánticos, relleno, concatenaciones vacías, productores negativos y fases sobre vistas anteriores.
Las copias y concatenaciones vacías usan palabras de 16/32 bits con y sin signo. Las concatenaciones
no vacías comprobadas usan palabras de 16 bits; los préstamos de concatenación comprobados parten de
raíces y conservan dos vistas booleanas antiguas. Los préstamos de copia conservan una palabra
antigua completa de 16 bits.

El resumen enumera por separado catorce combinaciones excluidas por el presupuesto de 128 cables y
espacio de trabajo. Entre ellas están los préstamos de concatenación con productor y los perfiles
grandes de 32 bits. Tampoco se califican todas las longitudes, todos los alias ni todos los estados
posibles. Las vistas comprobadas tienen límites de 64 bytes, además de los límites de expresiones,
puertas e historial. Las cadenas sobre heap, las vidas y ABI generales, la integración cotidiana
completa, los flotantes cuánticos y la recalificación de los papers generales siguen pendientes. La
ciberseguridad permanece clásica.

Esta nota describe la implementación; los resultados terminales y las fuentes exactas de cada
campaña se conservan con su evidencia. No afirma ejecución física en una QPU ni completitud de C11.
