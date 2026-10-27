# SDK con retornos CPU anticipados

Esta candidata RC3 añade `return;` anticipado en procedimientos cuánticos con bloques y bucles CPU.
Conserva los préstamos de lectura y las entradas de función comprobadas. Los SDK anteriores
mantienen sus archivos e identidades. Usa WSL `Ubuntu2204` sin montar unidades Windows.

Desde la raíz del repositorio:

```cmd
output\sdk\cpu-returns\run_sdk_demo.cmd
output\sdk\cpu-returns\run_return_demo.cmd
output\sdk\cpu-returns\compile_and_run.cmd output\sdk\cpu-returns\example_heap.c
```

El primer comando ejecuta los 37 programas reconstruidos de los seis bloques. El segundo compila el
ejemplo nuevo y comprueba visitas, resultado e interferencia; con el límite predeterminado imprime
`limit=2 visited=2 result=0 interference=0`. El tercero compila el ejemplo del heap.

`compile_and_run.cmd` admite un archivo C autónomo sin argumentos de programa. Lo compila en `-O2`,
lo ejecuta con el motor disperso y semilla 7 y reemplaza `user/program.c`. Requiere Linux
x86-64/glibc y GCC 11.4.0 (`gcc-11`). Para probar otros argumentos se puede ejecutar el programa
directamente desde el paquete en Linux.

El archivo conserva 242 entradas de fuente, binarios, headers y circuitos estáticos admitidos. La
extracción comprueba SHA256; `sdk-audit.json` identifica fuentes y flujos verificados. Incluye la
referencia de memoria runtime corregida. La
[nota de retornos](../../../docs/c11-procedure-returns.md) y su
[evidencia](../../../docs/audit-evidence/procedure-returns-v1/README.es.md) explican limpieza, fases
e inversas, y distinguen los resultados finitos de la integración general pendiente.

Los retornos de valor mantienen sus contratos anteriores. Retornos condicionados por datos
cuánticos, saltos generales y ABI/punteros/vidas arbitrarios requieren más trabajo, al igual que
flotantes cuánticos y composición completa. El modo denso admite 24 qubits; el disperso y la
representación llegan a 128 cables con presupuestos de soporte y trabajo. No se afirma una 1.0
estable, ejecución en QPU física ni aceleración. La ciberseguridad permanece clásica.

Las llamadas que necesitan la bandera CPU de retorno todavía se rechazan desde un `if` cuántico.
