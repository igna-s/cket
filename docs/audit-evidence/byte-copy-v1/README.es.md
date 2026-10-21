# Copia de bytes y sobrescrituras con historia

`memcpy` y `memmove` amplían su adaptación parcial: los destinos enteros desconocidos conservan
historia, y una ruta de hasta 64 bytes admite palabras parciales, tamaños enteros diferentes y
fuentes CPU conocidas. La ruta anterior de elementos completos conserva su optimización cuando el
destino es una base independiente conocida. El [contrato](../../c11-memcopy.md) distingue ambas
rutas, sus recursos y sus pendientes.

La entrega congela **180 entradas de compilador**. Incluye ocho programas nativos y sus circuitos
QASM e informes de recursos. `byte_copy` compara con libc un movimiento solapado, una sobrescritura
con historia y dos escrituras parciales de un entero, incluida una fuente CPU. Las muestras por
semilla y la regresión nativa normal quedan en `summary.json`.

La primera tentativa del ejemplo se conserva con sus fuentes y su salida real: fue rechazada por
medir un origen antes de terminar de usar su consumidor. Se corrigió el orden de las mediciones y
los casts de sus punteros; la restricción de dependencias sigue activa.

Se actualizan siete contratos mantenidos que antes esperaban rechazos y ahora corresponden a
operaciones implementadas. La matriz completa de memoria no se presenta como ejecutada: estados
completos, inversas, sanitizadores, contextos GCC, mutación CPU y la integración amplia permanecen
aplazados al cierre del bloque. Las campañas anteriores conservan su identidad de fuentes.

Siguen siendo **40 nombres de funciones parcialmente adaptadas**, con 769 entradas pendientes, 87
parciales y 368 de metadatos. No se cuenta C11 completo, clonación independiente de estados,
ejecución en QPU física ni aceleración demostrada. La ciberseguridad permanece clásica.
