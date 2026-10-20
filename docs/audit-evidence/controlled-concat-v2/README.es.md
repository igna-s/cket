# Concatenación bajo control cuántico

`strcat` y `strncat` admiten ahora llamadas dentro de un `if` cuántico con auxiliares privados
inicialmente en cero. Los auxiliares se reservan independientemente de la condición; sus valores y
las escrituras se calculan con los controles coherentes. Las restricciones sobre las declaraciones
cuánticas del usuario y las vidas dentro de bucles CPU dinámicos siguen vigentes. El
[contrato](../../c11-string-concat.md) conserva los límites y pendientes.

La entrega congela **178 entradas de compilador**. La compilación y el runner terminan con código 0:
siete programas nativos, siete circuitos QASM y siete informes de recursos. El ejemplo nuevo compara
los bytes con libc y preserva la fuente; las muestras de ambas ramas y la regresión nativa normal
están en `summary.json`. Una segunda secuencia usa concatenación controlada, fase y Fourier; su
resultado esperado y observado es `interference=1`.

La cápsula conserva también la primera tentativa: el programa nativo funcionó, pero la exportación
terminó con código 1 porque la referencia clásica usaba un `if` con un resultado medido. La segunda
tentativa selecciona el argumento de la referencia mediante una expresión. La protección del
exportador frente al control CPU dinámico permanece activa. Cada tentativa conserva sus fuentes,
logs y códigos de salida reales.

Este avance amplía contextos de funciones existentes: siguen siendo **40 nombres parcialmente
adaptados**, con 769 entradas pendientes, 87 parciales y 368 de metadatos. No acredita C11 completo.
Las matrices de estados completos, inversas, sanitizadores y compilación siguen aplazadas al cierre.
Las muestras finitas no certifican historias de sobrescritura arbitrarias ni todos los controles. No
se afirma ejecución en QPU física ni aceleración sobre CPU. La ciberseguridad sigue clásica.
