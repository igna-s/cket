# Inversas de temporales después de un intercambio controlado

Nota técnica de la candidata `1.0.0-rc.1`, 25 de octubre de 2026. Explica la corrección y su
evidencia finita; no constituye una prueba de corrección de todo el compilador.

## La obligación de limpieza

Calcular `t=f(a)` en un registro cero implementa, sobre valores de la base computacional,
`|a,0> → |a,f(a)>`. El resultado puede quedar entrelazado con su entrada. Para liberar `t` hay que
aplicar su inversa con los valores y controles que lo produjeron. Conservar `t` indefinidamente
altera el estado reducido de los registros públicos cuando esa correlación ya debía haberse
deshecho.

Una fase aplicada mientras `t` está disponible permanece como fase relativa después de limpiar `t`.
La limpieza no debe borrar esa fase ni leer las amplitudes para reconstruir las entradas.

## Dónde quedó el valor anterior

Sea `S` la conjunción de controles de un intercambio entre dueños `A` y `B`. Después del
intercambio, sus valores son `A'` y `B'`. Las entradas anteriores se reconstruyen como:

```text
oldA = S ? B' : A'
oldB = S ? A' : B'
```

Si la definición guardada era `F(A,B,R)`, con las demás entradas `R` todavía válidas, la inversa
debe usar `F(oldA,oldB,R)`. Estas igualdades valen para cada etiqueta de la base. Por linealidad
valen también para superposiciones con fases y entradas entrelazadas, bajo las mismas precondiciones
de tipos, controles y versiones. No afirman que los estados locales sean independientes ni que se
clone un estado arbitrario.

El motor prepara la sustitución de recetas antes de emitir las puertas del intercambio. Comprueba
las épocas de las dependencias, la disponibilidad de los dueños, la ausencia de ciclos y los
presupuestos de expansión. Las máscaras de controles guardadas que atraviesan el intercambio también
se expresan mediante las vistas anteriores. Las épocas de las entradas intercambiadas se actualizan;
el valor físico de un temporal prestado no cambia durante el transporte.

Un control privado derivado de raíces que no cambian se reconstruye desde esas raíces en la receta,
para permitir limpiar su copia privada. Si depende del destino que va a cambiar, hay que conservar
la información necesaria y evitar una dependencia circular. Retener una copia redundante del control
puede introducir una correlación observable incluso antes de salir del bloque; la regresión
comprueba también ese punto intermedio.

## Síntesis y certificados

Para ramas puras cuya validez se demuestra por bits, un selector booleano puede sintetizarse sin
copiar una palabra completa:

```text
select(s, yes, no) = no XOR (s AND (yes XOR no))
```

Ambas ramas deben pasar las comprobaciones de validez de esa ruta. Las expresiones que requieren
evaluación condicional para evitar comportamiento indefinido usan la síntesis aritmética
correspondiente. Las comparaciones entre booleanos devuelven `int` en C; sus valores siguen siendo
cero o uno, lo que permite reconstruir esos controles sin una palabra auxiliar.

Los certificados `support_interval` representan soporte numérico no negativo. Los patrones de bits
de enteros negativos no deben marcarse como intervalos no negativos. La unión conservadora de
soporte de un intercambio conserva esa distinción; de otro modo una inversa podría descartar
entradas negativas válidas.

## Evidencia y alcance

La [cápsula de esta revisión](../audit-evidence/rebound-borrows-v1/README.es.md) conserva entradas
exactas, comandos, herramientas y resultados terminales. Compara el estado complejo conjunto antes,
durante y después del préstamo; incluye fases independientes, controles abiertos y anidados,
préstamos encadenados y movidos, aritmética, una función del mismo archivo y preparaciones
negativas. Exige dueños públicos estables, historia necesaria y auxiliares limpios; además invierte
independientemente todo el flujo de puertas. Se usan ambas políticas de síntesis y construcciones
normales y ASan/UBSan.

Se califican asignaciones bool y de 8/16/32 bits y copias/rellenos de un byte hacia palabras enteras
en esos perfiles. Las pruebas del motor amplían los intercambios y rechazos. No se califican por
ello todas las escrituras repetidas, destinos derivados, pruebas de rangos correlacionados ni
préstamos de 64 bits. La copia coherente no idéntica al heap conserva un contraejemplo abierto. El
QASM generado del ejemplo es un artefacto de exportación; no se declara aquí una nueva validación
independiente de ese QASM en otro SDK.

Los papers anteriores y sus PDF conservan sus fuentes originales. Esta nota no amplía sus
afirmaciones ni cierra la auditoría general. La ciberseguridad permanece clásica.
