# Préstamos de lectura compartidos y casts

26 de octubre de 2026. Corrección posterior a RC3; la versión estable sigue pendiente.

El compilador anterior rechazaba dos argumentos de solo lectura sobre el mismo registro. Al mismo
tiempo, un único parámetro de puntero `const` podía modificarse mediante un cast: el contraejemplo
nativo transforma un elemento falso en verdadero, contradiciendo el contrato de préstamos cuánticos.
La cápsula conserva ambos comportamientos anteriores y sus fuentes.

Ahora se permite compartir préstamos de lectura y se registra por separado qué préstamos pueden
escribir. Si algún parámetro solapado es mutable, se conserva el rechazo en cualquier orden. La
comprobación de asignaciones e incrementos se realiza mientras sigue activo el préstamo del callee,
antes de sustituir las vistas por los registros del caller. La dirección de un parámetro escalar
conserva la misma protección. Las comprobaciones de consumo usan el conjunto completo de préstamos,
incluidos los de lectura; no se habilitan movimientos de propiedad ni clonación.

## Evidencia de esta revisión

Se congelan 239 entradas. Cambian el frontend, la integración del nuevo validador y el ejemplo; el
motor cuántico, driver y runtime conservan sus binarios exactos. Se reconstruyen el plugin y el
nuevo validador en normal y con ASan/UBSan. En cada configuración pasan:

- 30 compilaciones IR y 30 ejecuciones nativas, de seis programas en cinco niveles GCC.
- 60 estados completos contra fórmulas C, con fases relativas y auxiliares en cero.
- 60 estados evolucionados por otro intérprete de puertas y 60 inversas completas independientes.
- 65 rechazos con diagnóstico exacto: solapamiento mutable, consumo y casts de escritura.
- La regresión de bindings, 519 comprobaciones, y el núcleo nativo, 395 pruebas.

Los estados recorren las cuatro entradas de dos booleanos en superposición. Incluyen alias
escalares, punteros iguales, sufijos solapados, vistas escalares/punteros y funciones encadenadas,
con ambas políticas de síntesis. La referencia unitaria admite únicamente H/X/fase y separa las dos
observaciones finales explícitas; no interpreta la medición como reversible. Estas inversas son de
la secuencia completa de puertas, no una prueba de toda inversa guardada de C.

El [ejemplo](../../../examples/readonly_borrows.c) usa fase e interferencia y produce
`scalar=1 vector=0,1`. El [SDK nuevo](../../../output/sdk/readonly-borrows/README.es.md) reconstruye
los 37 programas de los seis bloques y pasa sus 1509 comprobaciones normales. Los lanzadores Windows
se verifican por separado en `sdk-audit.json`.

`evidence.tar.gz` conserva fuentes, comandos, logs, hashes y los intentos anteriores. Uno detectó
que el intérprete de referencia intentaba invertir las observaciones finales; otro mostró que tomar
la dirección de un parámetro escalar también necesitaba conservar su ámbito de lectura. Esos fallos
se mantienen separados de la calificación final.

## Límites

La corrección se califica para estos programas finitos de booleanos y buffers fijos. No demuestra
todos los enteros, layouts, alias/vidas/ABI o combinaciones del heap. Los resultados anteriores
conservan sus propios snapshots; no se recalifican automáticamente para este plugin nuevo. No se
ejecuta todo `scripts/verify.sh`. Los cinco validadores instrumentados pendientes de la campaña
anterior y los demás gates de integración conservan ese estado. No hay una nueva calificación QuEST
completa, QPU física ni demostración de aceleración. La ciberseguridad sigue siendo clásica.
