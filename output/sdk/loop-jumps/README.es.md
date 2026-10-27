# SDK con break y continue CPU desde llamadores coherentes

27 de octubre de 2026. Compilador experimental `1.0.0-rc.3`, con 244 entradas identificadas por hash
en `sdk-audit.json`, binarios Linux x86-64 para GCC 11.4.0 (`gcc-11`) y 37 programas de seis
bloques. Los lanzadores Windows requieren WSL `Ubuntu2204`.

```bat
output\sdk\loop-jumps\run_coherent_demo.cmd
output\sdk\loop-jumps\run_sdk_demo.cmd
output\sdk\loop-jumps\compile_and_run.cmd output\sdk\loop-jumps\example_heap.c
```

El ejemplo coherente usa un `for` con `continue`, `break`, un local mutable y fases con signo. El
incremento ocurre tras `continue`; `break` termina el bucle. La salida predeterminada es
`skip=2 control=1 target=0`. El ejemplo de heap imprime `rounds=3 output=12 sum=30`.

Los lanzadores transfieren archivos por entrada estándar a un directorio Linux identificado por hash
y no montan unidades Windows. `compile_and_run.cmd` acepta un archivo C, sin argumentos de programa;
usa O2, simulación dispersa y semilla 7, y reemplaza `user/program.c`. El paquete funciona también
directamente en Linux y permite ejecutar allí programas con argumentos.

La [nota](../../../docs/paper/loop_jumps_note.es.md) y la
[evidencia](../../../docs/audit-evidence/loop-jumps-v1/README.es.md) distinguen salidas nativas de
estados, fases, auxiliares e inversas. Las decisiones de salto son CPU puras bajo control coherente;
las puertas conservan el control cuántico del llamador y los bloques llegan a su limpieza. Los
permisos privados son booleanos CPU, no observaciones de qubits.

Quedan decisiones de salto/iteración cuánticas, `continue` a través de `switch`, flujo y
vidas/alias/ABI generales, efectos CPU anidados, flotantes cuánticos e integración completa de estas
fuentes. No se demuestra terminación de todo C. Los simuladores clásicos evolucionan amplitudes
complejas; no se afirma C11 completo, una 1.0 estable, QPU física o aceleración. La ciberseguridad
permanece clásica.
