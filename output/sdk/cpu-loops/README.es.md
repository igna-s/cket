# SDK con locales privados y bucles CPU coherentes

27 de octubre de 2026. Compilador experimental `1.0.0-rc.3`, con 244 entradas identificadas por hash
en `sdk-audit.json`, binarios Linux x86-64 para GCC 11.4.0 (`gcc-11`) y 37 programas de seis
bloques. Los lanzadores Windows requieren WSL `Ubuntu2204`.

```bat
output\sdk\cpu-loops\run_coherent_demo.cmd
output\sdk\cpu-loops\run_sdk_demo.cmd
output\sdk\cpu-loops\compile_and_run.cmd output\sdk\cpu-loops\example_heap.c
```

El ejemplo coherente usa un bucle CPU con retorno, un contador privado, fases con signo y un
parámetro modificado por valor. Su salida predeterminada es `count=1 control=1 target=0`. Dos
iteraciones se cancelan; el retorno impide las restantes. El parámetro del llamador conserva su
valor. El ejemplo de heap imprime `rounds=3 output=12 sum=30`.

Los lanzadores transfieren archivos por entrada estándar a un directorio Linux identificado por hash
y no montan unidades Windows. `compile_and_run.cmd` acepta un archivo C, sin argumentos de programa;
usa O2, simulación dispersa y semilla 7, y reemplaza `user/program.c`. El paquete funciona también
directamente en Linux y permite ejecutar allí programas con argumentos.

La [nota](../../../docs/paper/private_cpu_loops_note.es.md) y la
[evidencia](../../../docs/audit-evidence/private-cpu-loops-v1/README.es.md) distinguen salidas
nativas de comprobaciones de estados, fases, auxiliares e inversas. Los enteros CPU privados pueden
cambiar dentro de la misma rama cuántica donde se declararon, mediante sentencias puras; no pueden
guardar información de una decisión cuántica adicional ni exponer su dirección.

Los bucles `for`, `while` y `do while` usan condiciones CPU puras. No se demuestra terminación para
todo C. Quedan condiciones y salidas cuánticas, efectos CPU anidados, `break`/`continue` coherentes,
vidas/alias/ABI generales, flotantes cuánticos e integración completa de esta ampliación. Los
simuladores clásicos evolucionan amplitudes complejas; no se afirma C11 completo, una 1.0 estable,
QPU física o aceleración. La ciberseguridad permanece clásica.
