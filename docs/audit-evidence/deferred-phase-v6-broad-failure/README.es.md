# Regresión amplia de fases v6: ejecución fallida conservada

La campaña terminó con salida 1. Cerró 57 comandos: 56 con salida cero y el último,
`sanitized-bindings`, con salida 1. No es una regresión amplia aprobada.

La misma prueba que ya existía para `n ? 1 / d : false`, con `n` falso y `d` cero, fue rechazada
como efecto CPU al compilar con sanitizadores. La revisión identificó que el control de volatilidad
se aplicaba antes de reconocer el wrapper de GCC. La corrección está en pruebas en un candidato
posterior.

Se conservan los registros completos de los comandos cerrados, el controlador, la salida final y los
hashes de las 143 entradas. Son las mismas fuentes de `deferred-phase-v6/sources.tar.gz`. La
fotografía anterior que decía «en curso» permanece intacta como registro histórico; este cierre
agrega su resultado real.

Los 3380 controles dirigidos y los estados independientes anteriores mantienen su alcance, pero no
justifican integrar este candidato frente a una regresión confirmada. El árbol principal sigue en
138 entradas. La ciberseguridad no cambia.
