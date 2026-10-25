# Memoria coherente del heap: candidata 1.0.0-rc.2

El contraejemplo original de copia no idéntica al heap conserva ahora la rama inactiva: pureza
conjunta sectorial 1 → 1 y error de densidad cero. Las escrituras parciales intercambian solamente
los bits escritos; el contraste del byte alto intacto también conserva pureza 1 → 1 en ambas ramas.
Las fuentes de copias solapadas usan sus recetas guardadas, y las escrituras con índice cuántico
usan controles por elemento. La [nota técnica](../../paper/heap_coherence_note.es.md) explica las
inversas.

**2670 comparaciones de densidad pública**, **2670 contrastes de estados conjuntos por evolución
independiente de puertas** y **852 inversas completas**. No son tres recuentos intercambiables. Son
1780/568 normales O0/O2 y 890/284 ASan/UBSan O1, con 97 programas por nivel y ambas políticas. Las
fórmulas semánticas incluyen historia virtual de los valores anteriores; no prescriben una misma
codificación física privada. Las fases, los préstamos vivos/después de su limpieza y los valores
retenidos tras `free` están incluidos. Los tipos y límites exactos figuran en `summary.json`.

Pasan 35871 aserciones del motor, incluidos intercambios parciales con bits intactos en
superposición, inversas y máscaras inválidas antes de mutar. Ambas construcciones pasan además heap
16878, identidades 1682, expresiones 3505, preflight 23, versiones 15 y núcleo 395. Se repiten las
matrices de préstamos y sobrescrituras con sus recuentos separados. El bloque 05 normal ejecuta
nueve programas y exporta un QASM estático; la demo nueva tiene otras cuatro ejecuciones por
construcción. No se declara una nueva validación independiente de ese QASM en otro SDK.

Las campañas terminan en dos segmentos. El primer arnés forzó `CKET_SIMULATOR=sparse` también en el
núcleo, que incluye un rechazo intencionado de 32 cables en modo denso: terminó 394/1. El
controlador de continuación restauró el simulador predeterminado y pasó 395/0, más los pasos que
faltaban. Se conservan el fallo, los comandos y ambos segmentos; no se repitieron las matrices ya
cerradas ni se presenta una única ejecución ininterrumpida.

**Sigue abierto un contraejemplo en buffers fijos:** una escritura parcial de un byte retiene
también el byte alto intacto; su pureza conjunta con la referencia cae de 1 a 0.5 en la rama activa,
sin mediciones ni resets. La ruta del frontend todavía no usa el intercambio parcial del heap. Las
vidas generales, escrituras repetidas, direcciones/ABI y el perfil cotidiano conservan otros
pendientes. No se afirma que este sea el único defecto restante. Papers y CI general siguen
abiertos.

`evidence.tar.gz` conserva 233 entradas exactas, herramientas y símbolos ASan/UBSan de siete
componentes, logs terminales, comandos, estados y reproducciones actuales. `earlier-attempts.tar.gz`
separa ensayos exploratorios con fuentes no congeladas: primer fallo del byte intacto, eliminación
incorrecta de un predicado, exceso de espacio del préstamo de 32 bits y un observador fijo sin
consumidor futuro. No forman parte de los resultados calificados. La ciberseguridad sigue clásica.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
build/validate_swap_rebind
python3 scripts/validate-heap-coherence.py --output build/new-heap-coherence --levels O0 O2
bash scripts/run-block.sh 05
```
