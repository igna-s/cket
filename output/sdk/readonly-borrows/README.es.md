# SDK con préstamos compartidos de solo lectura

Esta entrega experimental RC3 incorpora la corrección de alias `const` y de escrituras mediante
casts. Conserva el paquete anterior en el directorio padre. Requiere WSL `Ubuntu2204`, Linux
x86-64/glibc y GCC 11.4.0 (`gcc-11`) para compilar archivos nuevos.

Desde la raíz del repositorio:

```cmd
output\sdk\readonly-borrows\run_sdk_demo.cmd
output\sdk\readonly-borrows\compile_and_run.cmd output\sdk\readonly-borrows\example_readonly.c
```

La demo ejecuta 37 programas de seis bloques, reconstruidos con el plugin corregido. El segundo
comando compila un ejemplo de interferencia con alias escalares y de buffer; debe imprimir
`scalar=1 vector=0,1`. El lanzador también admite otro archivo C autónomo, sin argumentos de
programa; conserva la copia de trabajo en `user/program.c` dentro del SDK en WSL y la reemplaza en
cada invocación. Usa `-O2`, el motor disperso y semilla 7.

La extracción comprueba los hashes y usa entrada estándar, sin montar unidades Windows. Las 239
entradas de compilador y pruebas y los binarios tienen sus hashes en `sdk-audit.json`. El paquete
incluye las fuentes y los circuitos estáticos admitidos; los heaps y bucles CPU dinámicos no se
convierten en una exportación QASM estática general.

El [contrato de préstamos](../../../docs/c11-readonly-borrows.md) explica la corrección y la
[cápsula](../../../docs/audit-evidence/readonly-borrows-v1/README.es.md) conserva pruebas de
estados, fases, inversas y rechazos. La suite de ejemplos pasa 1509 comprobaciones normales; ese
conteo no representa cobertura completa de C ni de todos los estados cuánticos.

Siguen abiertos vidas/alias/ABI generales, flotantes cuánticos, perfiles excluidos por recursos y la
integración completa. El simulador denso admite 24 qubits; el disperso y la representación llegan a
128 cables con presupuestos de soporte y trabajo. No hay ejecución demostrada en QPU física ni
aceleración demostrada frente a CPU. La ciberseguridad permanece clásica.
