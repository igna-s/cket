# SDK con retornos CPU y llamadas coherentes

Candidata RC3 con predicados CPU puros de retorno dentro de un llamador cuántico. Conserva los
paquetes anteriores y sus fuentes. Requiere Linux x86-64/glibc y GCC 11.4.0 (`gcc-11`); usa WSL
`Ubuntu2204` sin montar unidades Windows.

Desde la raíz del repositorio:

```cmd
output\sdk\coherent-returns\run_sdk_demo.cmd
output\sdk\coherent-returns\run_coherent_demo.cmd
output\sdk\coherent-returns\compile_and_run.cmd output\sdk\coherent-returns\example_heap.c
```

El primer comando ejecuta los 37 programas reconstruidos de seis bloques. El segundo compila el
ejemplo nuevo: con el valor predeterminado imprime `count=1 control=1 target=0`. Verifica que la
fase global de la llamada se vuelva relativa bajo control y se observe mediante interferencia. El
tercero compila el ejemplo del heap.

El lanzador de compilación recibe un archivo C autónomo sin argumentos de programa. Compila en
`-O2`, ejecuta con simulador disperso y semilla 7 y reemplaza su copia `user/program.c`. Otros
argumentos se pueden probar ejecutando directamente el programa en Linux. La extracción comprueba
SHA256; `sdk-audit.json` identifica las 244 entradas, componentes y flujos verificados.

La [nota técnica](../../../docs/paper/coherent_returns_note.es.md), el
[contrato](../../../docs/c11-procedure-returns.md) y la
[evidencia](../../../docs/audit-evidence/coherent-returns-v1/README.es.md) separan las pruebas
finitas del trabajo pendiente. Las decisiones de retorno deben ser CPU y puras. Los efectos CPU
observables, salidas decididas por datos cuánticos, bucles coherentes y vidas/alias/ABI generales
siguen restringidos. Los `double` de los ángulos son clásicos; los flotantes cuánticos siguen
pendientes. No es una 1.0 estable ni C11 completo. No se afirma QPU física ni aceleración. La
ciberseguridad permanece clásica.
