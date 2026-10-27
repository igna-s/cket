# SDK con locales CPU privados y llamadas coherentes

27 de octubre de 2026. Compilador experimental `1.0.0-rc.3`, con 244 entradas de fuente
identificadas en `sdk-audit.json`. Incluye binarios Linux x86-64 para GCC 11.4.0 (`gcc-11`), 37
programas de los seis bloques y fuentes. Requiere WSL `Ubuntu2204` para los lanzadores Windows.

```bat
output\sdk\immutable-locals\run_coherent_demo.cmd
output\sdk\immutable-locals\run_sdk_demo.cmd
output\sdk\immutable-locals\compile_and_run.cmd output\sdk\immutable-locals\example_heap.c
```

El primer comando compila `example_coherent.c`: usa locales `unsigned` y `bool` privados, retorno
CPU y una fase controlada que se vuelve interferencia. La salida predeterminada es
`count=3 control=1 target=0`. El tercero imprime `rounds=3 output=12 sum=30`.

Los archivos se transfieren por entrada estándar a un directorio Linux identificado por hash; los
lanzadores no montan unidades Windows. El compilador funciona también directamente desde Linux.
`compile_and_run.cmd` acepta un solo archivo C, sin argumentos de programa; compila a O2, usa
simulación dispersa y semilla 7, y reemplaza el archivo temporal `user/program.c`.

La [nota técnica](../../../docs/paper/private_cpu_locals_note.es.md) y la
[evidencia](../../../docs/audit-evidence/immutable-cpu-locals-v1/README.es.md) distinguen estados
complejos, fases, auxiliares e inversas de los ejemplos de salida. Se permiten locales enteros
automáticos, puros, inicializados una vez, sin escrituras posteriores ni dirección tomada.

Las funciones, arrays, punteros, heap y biblioteca frecuente conservan sus contratos parciales.
Quedan locales mutables y bucles bajo control coherente, salidas cuánticas y vidas/alias/ABI
generales, flotantes cuánticos e integración completa de esta ampliación. Los simuladores son
clásicos y evolucionan amplitudes complejas; no se afirma QPU física, ventaja de velocidad, C11
completo ni una 1.0 estable. La ciberseguridad permanece clásica.
