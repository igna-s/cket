# Préstamos ante escrituras inactivas

**Progreso parcial.** El contraejemplo con `control=0`, un temporal `saved=y^1` y una escritura
condicionada conservaba el temporal como raíz y perdía pureza al salir del bloque. Ahora conserva la
definición invertible cuando los certificados tipados prueban que la escritura está inactiva: la
pureza pasa de 1 → 0.5 a 1 → 1 y se conserva el efecto de fase esperado.

`qmaterialize` acepta controles opcionales. Si se demuestra inactividad global, no elimina la
definición; si la escritura puede activarse, mantiene la conservación anterior. `qswap` permite
definiciones/préstamos sin consumirlos solo en una ruta demostrablemente inactiva. Los tipos,
identidades, controles y alias siguen validados antes de mutar. La prueba lee valores/épocas
guardados; no lee amplitudes ni realiza una medida o reset.

La regresión nueva registra **252 puntos de estado completo y 126 inversas completas**: 168/84
normales O0/O2 y 84/42 ASan/UBSan O1. Incluye asignaciones bool y de 8/16/32 bits, y copias de un
byte hacia palabras de 8/16/32 bits, ambas políticas de síntesis y controles cero, uno negado y cero
preparado desde un argumento CPU. Compara fases, dueños/cables estables y auxiliares cero después de
salir del bloque. Una primera copia de palabra completa de 32 bits excedió 128 cables; queda
archivada como límite, sin contarla como prueba aprobada. No se califica la limpieza de préstamos de
64 bits.

Pasan también 139 aserciones de certificados/guardas/épocas/rechazos del motor. Las campañas de
identidades del heap y sobrescrituras fijas se repiten con esta revisión y tienen recuentos propios
en `summary.json`. En ambas construcciones pasan además las regresiones del heap 16878, identidades
1682, operandos controlados 1270, núcleo 395 y memoria/historia 23/15. Son aserciones y contextos
finitos, sin equivalencia con un porcentaje de C. Los siete ejemplos del bloque 05 pasan en la
construcción normal.

**Persisten dos contraejemplos:** con control en superposición, el préstamo todavía pierde
coherencia en el sector original `control=0`; la copia coherente no idéntica al heap también
presenta ese defecto. Ambos tienen pureza sectorial 1 → 0.5, sin canales explícitos. La inversa
general necesita una vista condicionada del dueño anterior. Esta corrección no resuelve ese caso.

`evidence.tar.gz` conserva las 227 entradas exactas congeladas, logs terminales, comandos, hashes de
herramientas, inspección ASan/UBSan y los contrastes corregidos/abiertos. Los primeros errores de
arnés/backend y límites de recursos están en `earlier-attempts.tar.gz`, separados de los resultados
finales. Los papers y la nota técnica mantienen sus revisiones originales; su recalificación
completa sigue pendiente. La ciberseguridad permanece clásica y sin cambios.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
build/validate_inactive_borrows
python3 scripts/validate-inactive-borrows.py --output build/new-inactive-borrow-proof --levels O0 O2
```
