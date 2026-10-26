# Bytes intactos y préstamos en buffers fijos

Nota técnica de `1.0.0-rc.3`, 26 de octubre de 2026. Amplía la
[corrección del heap](heap_coherence_note.es.md) al adaptador de bytes de buffers fijos. Su
[evidencia](../audit-evidence/fixed-patches-v1/README.es.md) tiene un alcance finito.

## Contrato de la escritura

Sean `Y` la palabra anterior, `E` el valor calculado, `S` el control y `M` la máscara de bits
escritos, limitada al ancho de la palabra. Se calcula en un auxiliar cero:

```text
T = S ? (E & M) : 0
```

Se intercambian solamente los bits de `M`, bajo `S`:

```text
Y' = (Y & ~M) | ((S ? E : Y) & M)
H  = S ? (Y & M) : 0
```

La operación actúa como identidad sobre los cables fuera de `M`. Una copia de una palabra nueva
completa habría correlacionado también esos bits intactos con historia privada. Las expresiones
anteriores describen etiquetas de base; la secuencia de puertas se extiende linealmente a estados
con fases y entrelazamiento. No se consultan amplitudes ni se mide para decidir qué escribir.

`qpatch` recibe un destino, un auxiliar limpio distinto, una expresión tipada y una máscara `uint64`
tipada que ya fue capturada por la CPU. Admite máscara cero para longitudes vacías. La máscara
numérica de `qswap` conserva su contrato anterior: cero sigue siendo inválido. Los dueños públicos
mantienen identidad y cables. No se crean dos estados desconocidos independientes; el valor anterior
sobrescrito forma parte del estado conjunto retenido.

## Vista anterior e inversa

Los consumidores que todavía necesitan `Y` usan:

```text
oldY = (Y' & ~M) | ((S ? H : Y') & M)
```

El transporte de recetas conserva épocas, dependencias, controles y máscaras. Si `Y` era una raíz
desconocida, sus bits sobrescritos pueden necesitar historia persistente. Si tenía un productor
reversible válido, el runtime conserva su receta y fija sus entradas durante el cambio. Cuando los
préstamos terminan, puede deshacer `H` con ese productor. Un productor conocido no se convierte
automáticamente en historia irreversible.

Mientras vive un préstamo, un predicado y una historia derivados pueden seguir entrelazados con el
sistema público. La matriz registra ese estado intermedio explícitamente; después de la limpieza
comprueba su desaparición. No exige pureza intermedia cuando el contrato todavía necesita auxiliares
vivos, ni confunde la inversa global de puertas con una prueba suficiente de la semántica pública.

Las longitudes CPU se capturan una vez para cada escritura y se reutilizan en sus vistas anteriores.
Una rama CPU no ejecutada deja máscara cero, con alcance léxico válido fuera de esa rama. Cambiar la
variable de longitud después de copiar no cambia la receta guardada. El valor de un relleno con
alias se captura antes de modificar su primer destino.

## Síntesis y comprobaciones

Las comparaciones cerradas que quedan al enlazar longitudes CPU se evalúan como constantes tipadas.
Se exige que no tengan fuentes cuánticas; no se especializa el circuito leyendo el estado. Eso
permite sintetizar por bits los rellenos de 32 bits sin reservar palabras auxiliares innecesarias.
La prueba de identidad reconoce los rellenos que conservan el valor; los casos fuera de su prueba
acotada siguen la síntesis ordinaria y sus verificaciones de dominio.

La cápsula distingue fórmulas independientes de densidad pública, estados físicos reproducidos por
otro intérprete de puertas e inversión completa. Incluye raíces y productores, enteros con y sin
signo de 16/32 bits, controles abiertos/anidados, longitudes 0/1/2, alias, solapamiento,
transferencias desde heap y préstamos con fases. El observador de bucles registra máscaras y
ubicaciones de historia; la referencia calcula los valores anteriores sin leer amplitudes y sigue
comparando el estado conjunto completo.

Esto cierra el contraejemplo publicado del byte intacto en esos perfiles. No califica todos los
adaptadores de cadenas, vidas y alias generales, todos los estados o todo C11. Los perfiles de 64
bits siguen sujetos a recursos y requieren calificación separada. Los PDF anteriores conservan sus
fuentes históricas. La ciberseguridad permanece clásica y sin cambios.
