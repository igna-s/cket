# Memoria cuántica con longitudes clásicas en C Ket

Nota técnica experimental, 24 de octubre de 2026.

Código de la entrega: `da2ffb31e568026d58b8e5d98aa31929171cfac0`.

## Resumen

Se presenta una entrega nativa que combina funciones, arrays enteros y las operaciones `memcpy`,
`memmove` y `memset` con una longitud CPU recibida durante la ejecución. El contenido reside en
registros cuánticos simulados; las direcciones y longitudes son clásicas. El ejemplo
`buffer_lengths.c` copia un prefijo, mueve bytes solapados y rellena un prefijo, observa los
resultados explícitamente y verifica fórmulas C. Esta nota corresponde a la campaña
`runtime-memory-v1`, con fuentes identificadas mediante SHA-256.

## Modelo y correcciones

Una comprobación `cmemory` valida límites y solapamiento antes de modificar destinos. Las escrituras
calculan versiones nuevas y conservan la información necesaria para sus inversas. Los dueños
públicos permanecen estables entre ramas; las recetas concretas siguen las identidades privadas
retiradas. Los bytes fuera del prefijo mantienen su valor y las palabras enteras no seleccionadas
conservan sus cables físicos. El relleno se captura una vez, incluso para longitud cero. No se
insertan mediciones ni resets para borrar historia desconocida.

La extensión es una transformación reversible sobre valores de base con información retenida. No
crea dos copias independientes de un estado cuántico desconocido. Se corrigieron una unión de ramas
que invalidaba un dueño del otro brazo y una lectura del retorno de memoria que podía leer un
placeholder CPU. En la reproducción original se obtuvo `0` frente al valor cuántico observado `3`.
La observación explícita corregida obtiene el registro real; una lectura CPU sin `measure` y los
escapes no comprobados se rechazan.

## Validación reproducible

La campaña congela las mismas 221 entradas para construcciones normales y ASan/UBSan con GCC 11.4.0.
Sus cinco perfiles ejercitan copias, movimientos en ambos sentidos, relleno que lee su destino y
bytes parciales, con longitudes 0, 1 y 2 y bucles de 0, 1 y 3 iteraciones. Se comparan 900 puntos de
estado complejo conjunto con fórmulas independientes y con evolución independiente de puertas; 270
recorridos invierten el flujo completo hasta el estado inicial. Son perfiles finitos repetidos en
`O0`/`O2` normales y `O1` instrumentado, con ambas políticas de síntesis.

Los estados, comandos, resultados, hashes y categorías de regresión se conservan en la cápsula de
evidencia. Los 23 contratos de preflight por configuración comprueban esquemas, límites,
solapamiento, tipos vinculados y controles inactivos antes de mutar. Las ejecuciones nativas, los
rechazos y las comprobaciones de identidad se contabilizan aparte de los puntos anteriores.

## Programa y reproducción

La fuente completa es `examples/blocks/05/buffer_lengths.c`. La distribución nativa para Linux
x86-64 incluye su ejecutable, fuente, hashes y un lanzador para Windows con WSL. El programa acepta
longitudes de 0 a 2 y devuelve un código de error si sus fórmulas no coinciden.

```sh
CC=gcc-11 CXX=g++-11 bash scripts/build.sh
bash scripts/run-block.sh 05
CKET_SIMULATOR=sparse build/blocks/05/buffer_lengths 2
```

## Límites

La entrega conserva direcciones y extensiones comprobadas, hasta 64 bytes por operación y el límite
de representación de 128 cables. La simulación densa admite 24 cables; el modo disperso tiene
soporte ocupado acotado. Faltan direcciones/ABI generales, más flujos de propiedad, cadenas sobre
heap y flotantes cuánticos. El C cotidiano sigue en desarrollo. Esta validación usa un simulador, no
una QPU física, y no demuestra aceleración. Ciberseguridad permanece clásica.

Esta nota agrega evidencia de la entrega actual. Los manuscritos históricos en español e inglés
conservan su revisión y resultados originales; su revisión integral con el compilador nuevo sigue
pendiente.

## Fuentes de esta nota

1. Fuente ejecutada: `examples/blocks/05/buffer_lengths.c` en la cápsula de 221 entradas.
1. Evidencia: `docs/audit-evidence/runtime-memory-v1/summary.json` y `evidence.tar.gz`.
1. Contrato: `docs/c11-runtime-memory.md`.
