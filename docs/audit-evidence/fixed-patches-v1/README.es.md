# Escrituras parciales de buffers fijos: 1.0.0-rc.3

El contraejemplo del byte alto intacto publicado en RC2 pasa ahora en ambas ramas: pureza conjunta 1
→ 1 y error máximo de densidad cero. Se archivan el programa y los estados sobre la construcción
final. La [nota técnica](../../paper/fixed_patches_note.es.md) explica máscaras y recetas
anteriores.

La matriz nueva registra **1920 densidades públicas**, **1920 estados conjuntos por evolución
independiente de puertas** y **912 inversas completas**: 1280/608 normales O0/O2 y 640/304
ASan/UBSan O1, con 72 programas por nivel y ambas políticas. Los recuentos tienen ámbitos
diferentes.

Incluye raíces y productores, enteros con/sin signo 16/32, controles abiertos/anidados, longitudes
0/1/2, rellenos con alias, movimientos solapados dentro de una palabra, heap→fijo y préstamos con
fases. Cambiar la longitud CPU después de escribir no modifica el valor anterior. Los predicados
temporales vivos se incluyen explícitamente en la referencia; se comprueba su limpieza posterior. La
historia irreversible contiene solamente los bits sobrescritos de raíces desconocidas.

Pasan además 24978 aserciones directas de `qpatch`, incluyendo identidad de historia cero válida,
máscara cero, estados con fases, productores negativos, limpieza, inversas completas y rechazo de
máscaras inválidas antes de mutar. Las regresiones de cada construcción incluyen intercambios 35871,
heap 16878, identidades 1682, expresiones 3505, memoria preflight 23, versiones 15, copia 414,
relleno 940 y núcleo 395. Las matrices de memoria runtime, bucles fijos y heap se registran por
separado en `summary.json`. El bloque 05 normal ejecuta diez programas y exporta un QASM; esa
exportación no representa una nueva validación con un SDK independiente. La demo tiene ocho
ejecuciones por construcción.

`evidence.tar.gz` conserva 236 entradas exactas, comandos y logs terminales, construcciones y
símbolos ASan/UBSan de siete componentes. `earlier-attempts.tar.gz` conserva ensayos exploratorios y
el primer corte por un observador antiguo. El observador nuevo registra máscaras y ubicación del
historial, sin leer amplitudes para decidir valores esperados; las fórmulas de bucles continúan
comparando el estado conjunto completo. La campaña normal termina en un segmento. La instrumentada
se interrumpió después de trece comandos exitosos y se completó en una continuación del núcleo y la
demo. Se conservan ambos segmentos; no se repitieron las matrices ni memset ya terminados.

El segundo corte encontró una regresión real del relleno estático con alias: su captura prestaba el
dueño que el camino antiguo cambiaba. Se corrige con intercambio enmascarado, preservando el camino
sin historia de destinos conocidos. Tres perfiles de memset exigen ahora identidad y limpieza reales
en estados dispersos completos y sus inversas; no exigen un duplicado innecesario del dato. Los
demás perfiles conservan sus pruebas densas o de todas las etiquetas de base.

No se califica toda C11, toda la biblioteca ni todo el perfil cotidiano. Vidas y alias generales,
escrituras de cadenas con selección cuántica, recursos de 64 bits y composición requieren más
trabajo. Papers y CI general de esta revisión siguen pendientes. Los PDF históricos conservan su
revisión. La ciberseguridad permanece clásica y sin cambios.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
build/validate_patches
python3 scripts/validate-fixed-patches.py --output build/new-fixed-patches --levels O0 O2
bash scripts/run-block.sh 05
```
