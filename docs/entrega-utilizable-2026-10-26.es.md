# Entrega utilizable de C Ket

Actualizado el 27 de octubre de 2026. Compilador experimental **1.0.0-rc.3**, con las correcciones
publicadas. La 1.0 estable y el núcleo cotidiano completo continúan pendientes.

## Usarlo ahora

Desde la raíz del repositorio en Windows:

```cmd
output\sdk\checked-entry\run_sdk_demo.cmd
output\sdk\checked-entry\run_checked_demo.cmd
output\sdk\checked-entry\compile_and_run.cmd output\sdk\checked-entry\example_heap.c
```

El primer comando ejecuta 37 programas reconstruidos. El segundo compila un ejemplo con alias de
lectura y comprueba interferencia: `scalar=1 vector=0,1`. El tercero compila un programa de heap.
Podés editar una copia y pasar su ruta al mismo lanzador. La
[guía del SDK actualizado](../output/sdk/checked-entry/README.es.md) explica los requisitos y la CLI
en Linux. Usa WSL `Ubuntu2204` sin montar unidades Windows. El SDK anterior se conserva en el
directorio padre con su propia identidad.

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

El SDK actualizado conserva las 240 entradas exactas del compilador y pruebas, ejecutables, headers
y fuentes. Los seis runners reconstruyen los 37 programas y sus exportaciones estáticas admitidas.
La suite de ejemplos pasa 1509 comprobaciones normales en cinco niveles de GCC. Los lanzadores
Windows ejecutan el paquete y compilan el ejemplo nuevo de alias e interferencia.

La [corrección de entradas de función](c11-function-entries.md) añade 90 contratos normales e
instrumentados para llamadas, prototipos y punteros. Evita que una llamada clásica no comprobada
termine en un stub vacío.

El fallo anterior `unmodelled retained version` se reproduce y corrige en la
[referencia de memoria runtime](audit-evidence/runtime-memory-reference-v1/README.es.md). Pasan 900
puntos de estado complejo y 270 inversas completas en normal y ASan/UBSan; tres mutaciones unitarias
de datos, fase y guarda se rechazan. Solo cambia el validador Python del checkout: el compilador y
los binarios del SDK siguen iguales. El archivo del SDK conserva su validador y hashes originales.
Esta corrección dirigida no equivale a una ejecución general de CI aprobada.

La [corrección de préstamos](c11-readonly-borrows.md) permite alias de lectura y rechaza escrituras
mediante casts. Su [cápsula](audit-evidence/readonly-borrows-v1/README.es.md) registra 305
comprobaciones en normal y ASan/UBSan: compilaciones, ejecuciones, estados completos contra fórmulas
C, evolución independiente de puertas, inversas y diagnósticos. También pasan bindings (519) y
núcleo (395) en ambas configuraciones. Estos programas finitos no prueban todos los alias, vidas o
tipos.

Las campañas anteriores de 237 entradas conservan sus fuentes: la
[referencia de retornos](audit-evidence/memory-return-reference-v1/README.es.md), la
[integración C prioritaria](audit-evidence/c-priority-integration-v1/README.es.md) y la
[continuación de locale](audit-evidence/c-locale-sanitized-v1/README.es.md). Entre esas dos últimas
pasaron 17 validadores normales y doce completos instrumentados; quedaron cinco pendientes. Sus
cifras de fuzzer, QuEST y memoria no recalifican automáticamente este plugin nuevo. La ejecución
completa de `scripts/verify.sh` sigue pendiente.

## Qué falta

Quedan vidas, alias, punteros y ABI generales, más dominios de división y recursos, flotantes
cuánticos y el cierre de la integración completa. Hay perfiles aceptados por el frontend que superan
el presupuesto o no tienen síntesis implementada. El modo denso admite 24 qubits; el disperso y la
representación llegan a 128 cables con límites de soporte y trabajo.

Los manuscritos generales conservan su evaluación histórica; las notas nuevas describen los bloques
actuales. No hay demostración de QPU física ni de aceleración frente a CPU. Esta entrega permite
usar los bloques comprobados y compilar programas propios dentro de sus contratos; no declara todo
C11 ni toda libc cuánticamente implementados. La ciberseguridad permanece clásica.
