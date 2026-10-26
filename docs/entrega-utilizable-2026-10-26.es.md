# Entrega utilizable de C Ket

26 de octubre de 2026. Compilador experimental **1.0.0-rc.3**, con las correcciones publicadas. La
1.0 estable y el núcleo cotidiano completo continúan pendientes.

## Usarlo ahora

Desde la raíz del repositorio en Windows:

```cmd
output\sdk\run_sdk_demo.cmd
output\sdk\compile_and_run.cmd output\sdk\example_functions.c
output\sdk\compile_and_run.cmd output\sdk\example_heap.c
```

El primer comando ejecuta 37 programas ya compilados. Los otros compilan y ejecutan archivos C
mediante el compilador incluido. Podés editar una copia de esos archivos y pasar su ruta al mismo
comando. La [guía del SDK](../output/sdk/README.es.md) explica los requisitos y la CLI en Linux. Usa
WSL `Ubuntu2204` sin montar unidades Windows.

## Qué podés escribir dentro del perfil actual

| Área                    | Ejemplos disponibles                                                                                                    |
| ----------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| Tipos y operadores      | Booleanos, bytes, enteros con/sin signo, conversiones, aritmética y desplazamientos, con límites de dominio y recursos. |
| Funciones y arrays      | Parámetros prestados, resultados escalares, recorridos CPU, acumulación y actualizaciones de buffers.                   |
| Punteros e índices      | Alias comprobados, vistas de bytes, índices CPU y cuánticos dentro del objeto y operaciones frecuentes de memoria.      |
| Almacenamiento dinámico | `calloc`, `malloc`, `realloc`, `free`, recorridos y versiones por iteración dentro del contrato del heap.               |
| Biblioteca frecuente    | Adaptadores parciales de copia, movimiento, relleno, cadenas, búsqueda y conversiones.                                  |
| Operaciones cuánticas   | Superposición, entrelazamiento, control coherente, fases, Fourier y medición explícita.                                 |

Las direcciones, capacidades, entrada/salida y decisiones CPU siguen siendo clásicas. Los datos
cuánticos usan registros y puertas en el simulador C de amplitudes complejas. Copiar valores de base
puede entrelazarlos; no produce dos copias independientes de un estado desconocido. Sobrescribir
conserva la información necesaria como historia o la limpia mediante una inversa justificada. La
[explicación de propiedad](no-clonacion.es.md) desarrolla esos contratos.

## Qué se comprobó en esta entrega

El SDK conserva las 237 entradas exactas del compilador y pruebas, ejecutables, headers y fuentes.
Los seis runners produjeron los 37 programas y las exportaciones estáticas admitidas. La suite de
ejemplos pasó 1509 comprobaciones en cinco niveles de GCC. El lanzador de Windows ejecutó el paquete
y compiló de nuevo los ejemplos de funciones y heap.

Las referencias de retornos de memoria y distancias pasan 290 y 170 comprobaciones en normal y
ASan/UBSan. La [cápsula](audit-evidence/memory-return-reference-v1/README.es.md) distingue valores
nativos, acciones etiquetadas completas, fases, canales de historial e inversas. Las salidas medidas
de la demo no se presentan como prueba de todos los estados posibles.

La [campaña C posterior](audit-evidence/c-priority-integration-v1/README.es.md) pasa sus 17
validadores normales y diez completos con ASan/UBSan, incluido el historial completo de 2332 casos.
La pasada instrumentada de capturas de bytes queda interrumpida por tiempo; los seis validadores que
venían después permanecen pendientes. Además pasan en normal 145 compilaciones del fuzzer, 24
circuitos de crecimiento y las 70 exportaciones verificadas por QuEST: 64 estados y seis canales. La
corrección posterior afecta solo al test QASM; el paquete conserva los mismos binarios del
compilador y su snapshot original. La campaña no es una ejecución completa de `scripts/verify.sh`.

## Qué falta

Quedan vidas, alias, punteros y ABI generales, más dominios de división y recursos, flotantes
cuánticos y el cierre de la integración completa. Hay perfiles aceptados por el frontend que superan
el presupuesto o no tienen síntesis implementada. El modo denso admite 24 qubits; el disperso y la
representación llegan a 128 cables con límites de soporte y trabajo.

Los manuscritos generales conservan su evaluación histórica; las notas nuevas describen los bloques
actuales. No hay demostración de QPU física ni de aceleración frente a CPU. Esta entrega permite
usar los bloques comprobados y compilar programas propios dentro de sus contratos; no declara todo
C11 ni toda libc cuánticamente implementados. La ciberseguridad permanece clásica.
