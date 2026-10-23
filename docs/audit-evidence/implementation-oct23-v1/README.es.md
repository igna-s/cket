# Implementación del 23 de octubre, validación aplazada

Se añaden adaptadores parciales de `atoi`, `atol` y `atoll` y escrituras de caracteres mediante
punteros guardados de búsqueda. Estas últimas incluyen asignaciones simples/compuestas,
incrementos/decrementos y sus valores de expresión, con capturas previas e historia reversible. La
captura interna conserva el ancho solicitado, incluidos estados de magnitud de 64 bits.

Por indicación expresa del usuario, esta entrega registra compilación, sin ejecutar suites ni
ejemplos de la versión final. Todos los resultados de runtime, estados, inversas, locales,
sanitizadores e integración siguen pendientes. El prototipo decimal anterior sí ejecutó y se
conserva aparte con su rechazo de exportación sin locale; no certifica las fuentes finales.

La cápsula identifica las fuentes exactas y el resultado real de construcción. No se cuenta C11
completo: hay 43 nombres parcialmente adaptados, 766 entradas pendientes, 90 parciales y 368 de
metadatos. La ciberseguridad permanece clásica. Los papers conservan su evaluación histórica; sus
informes actuales necesitan una nueva ejecución de auditoría al final.
