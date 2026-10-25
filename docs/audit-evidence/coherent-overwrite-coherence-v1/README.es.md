# Hallazgo de coherencia en sobrescrituras condicionadas

**Estado: reproducido y abierto.** La revisión usa las 221 entradas y las herramientas de la entrega
`da2ffb31e568`. Hay cuatro contraejemplos de pérdida de coherencia del destino en la rama inactiva:
copia y asignación, con control cero y con control coherente proyectado a cero. La pureza baja de 1
a 0.5 y el error máximo de matriz de densidad es 0.5, sin medición ni reset.

`evidence.tar.gz` conserva fuentes, ambos observadores Python, programas generados, comandos/logs y
hashes de las herramientas. El observador C enlaza el runtime real y no reemplaza sus operaciones.
Los scripts exploratorios conservan rutas absolutas originales; para repetirlos, ajustar `root` y
usar un directorio nuevo de salida. El compilador se reconstruye desde `sources` con GCC 11.4.0.

La exploración adicional contiene 30 estados conjuntos complejos y 10 inversas de puertas sobre
cinco perfiles, con amplitudes complejas no uniformes y fuentes/destinos independientes. Esos casos
pasan en O2 sin tablas y no subsanan los contraejemplos. Los sondeos locales del heap empiezan con
un destino ya mixto y no demuestran identidad de su estado conjunto con otros registros.

`earlier-harness-attempts.tar.gz` separa intentos que se detuvieron por auxiliares aún no modelados
o por el contrato de retornos CPU; sus recuentos no se suman a los resultados finales.

El [informe](../../re-audit-2026-10-24-coherence.md) explica la causa y el alcance. La demo
publicada no contiene esta combinación. La ciberseguridad permanece clásica.
