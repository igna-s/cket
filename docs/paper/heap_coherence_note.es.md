# Escrituras parciales del heap e inversas de sus fuentes

Nota técnica de `1.0.0-rc.2`, 25 de octubre de 2026. Describe una corrección finita y un hallazgo
que sigue abierto en buffers fijos. No es una prueba de corrección de todo el compilador.

## Invariante de los bits que no se escriben

Una escritura bajo control `S` debe conservar el destino cuando `S=0`. Si escribe solamente los bits
de una máscara `M`, los demás bits del destino deben conservar su coherencia también cuando `S=1`.
Calcular una palabra nueva que copie los bits intactos y retener la palabra anterior puede añadir
una correlación privada que incumple ese segundo invariante.

Sean `A` y `B` los dueños de dos palabras del mismo tipo, con controles disjuntos de sus cables. El
intercambio parcial aplica el intercambio controlado solamente a las posiciones de `M`:

```text
A' = (A & ~M) | ((S ? B : A) & M)
B' = (B & ~M) | ((S ? A : B) & M)
```

La máscara está limitada al ancho de la palabra. Es la identidad sobre los bits que no selecciona.
Las entradas anteriores se reconstruyen con las mismas posiciones:

```text
oldA = (A' & ~M) | ((S ? B' : A') & M)
oldB = (B' & ~M) | ((S ? A' : B') & M)
```

Las igualdades valen sobre cada etiqueta de la base y se extienden por linealidad a superposiciones
con fases y datos entrelazados. El transporte de recetas de la
[nota anterior](conditional_inverse_note.es.md) usa estas vistas, valida épocas y ciclos y permite
limpiar los temporales desde la ubicación real de sus entradas anteriores.

El heap calcula el valor activo en un auxiliar cero, con ceros fuera de `M`. Después del
intercambio, el destino conserva sus cables. La historia contiene solamente los bits anteriores
escritos en amplitudes activas; no recibe otra copia de los bits intactos. No se leen amplitudes
para decidir la operación ni se introducen mediciones o resets. La representación privada de la
historia sigue siendo un recurso explícito.

`qswap` y `qheap_store` aceptan `bit_mask` opcional, entero no nulo dentro del ancho declarado.
Omitirla conserva el intercambio/escritura completos. La validación rechaza máscaras mal tipadas,
cero o fuera del ancho antes de esas operaciones. Los certificados de soporte del intercambio
parcial conservan ceros y cotas por dueño; los patrones negativos no se certifican como números no
negativos.

## Fuentes originales y decisiones

Una copia solapada necesita los bytes anteriores al primer cambio. El registro privado de retención
guarda una secuencia de lecturas cuyo resultado es cero. Sus recetas conservan las dependencias y se
transportan durante cada intercambio. Las lecturas siguientes consultan esas expresiones guardadas;
reutilizar solamente el identificador original leería un valor ya modificado.

Las escrituras con índice cuántico capturan el índice y el valor, calculan un predicado por elemento
y usan ese predicado como control. No calculan un destino nuevo con el valor viejo de cada elemento
no seleccionado. Las decisiones que dependen del destino requieren conservar la información anterior
y evitar ciclos. La eliminación del frontend delega en la receta efectiva del runtime, en lugar de
exigir una definición que una materialización ya retiró.

La síntesis por bits de una secuencia prueba la validez de **todo** el operando descartado antes de
usar su resultado. Así una lectura con índice CPU capturado evita una segunda palabra auxiliar, sin
omitir divisiones o desplazamientos inválidos. Si esta prueba pura no basta, se utiliza la ruta
aritmética que conserva la evaluación condicional y la comprobación de dominios.

## Evidencia y límites

La [cápsula](../audit-evidence/heap-coherence-v1/README.es.md) distingue las comparaciones de
densidad pública con fórmulas de historia, los estados físicos conjuntos reproducidos por otro
intérprete de puertas y la inversión completa de esas puertas. La representación privada no se
presupone idéntica entre algoritmos. Los puntos anteriores, posteriores, con préstamo vivo y después
de `free` incluyen fases independientes y verifican ausencia de canales implícitos.

Las construcciones normales y ASan/UBSan califican los programas y tipos indicados en la cápsula. No
califican toda la biblioteca, todos los estados, los circuitos de 64 bits que exceden recursos, la
vida general de punteros ni una QPU física. Los manuscritos y PDF anteriores conservan sus propias
fuentes y resultados.

**Buffers fijos conservan un contraejemplo abierto.** Su adaptador de escritura parcial todavía
puede retener una copia del byte intacto en la rama activa. El contraste conjunto de ese byte y su
referencia da pureza 1 → 0.5. La corrección de este documento se implementa en el heap y no se
generaliza a esa otra ruta. La ciberseguridad permanece clásica y sin cambios.
