# Índice maestro de arquitectura y código de `|c>` (`cket`)

Este índice mapea dónde está cada subsistema, archivo, función clave, instrucción del IR cuántico
(`cket.qir`) y validador del proyecto para consulta rápida sin tener que recorrer todo el árbol.

______________________________________________________________________

## 1. Mapa de inclusión y compilación

### 1.1 Plugin GCC C (`build/cket_plugin.so`)

Punto de entrada: \[src/cket_plugin.cpp\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp) (627
líneas).

- Registra atributos `__attribute__((cket_quant))`
  (\[`quantum_attribute`\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp#L231)) y
  `__attribute__((cket_intrinsic))`
  (\[`intrinsic_attribute`\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp#L290)).
- Hook principal: `PLUGIN_PRE_GENERICIZE` ->
  \[`finish_function`\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp#L435-L573).
- Pipeline en \[`finish_function`\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp#L453-L504):
  1. Si `quantum_function(fn)`:
     \[`capture_quantum_function`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L330).
  1. \[`expand_quantum_program`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1919)
     (expande productos, llamadas cuánticas, memoria, strings, índices, `for` estático).
  1. \[`normalize_quantum_saves`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L735)
     (captura snapshots `SAVE_EXPR` antes de efectos secuenciados).
  1. \[`expand_cpu_loop_jumps`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L605)
     (reescribe `break` y `continue` en `for`/`while`/`do` con banderas privadas
     `__cket_loop_active` y `__cket_iteration_active`).
  1. \[`discover_declarations`\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp#L408) ->
     \[`record_decl`\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp#L324).
  1. \[`count_uses`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L484) y
     \[`observed_storage_dependencies`\](file:///e:/Backup/GitHub/Quantum/src/quantum_lifetimes.inc#L88).
  1. \[`discover_coherent_cpu_locals`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L181)
     (identifica escalares CPU privados seguros dentro de `if (control)`).
  1. \[`lower_node`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L2580) (baja el AST
     de GCC a llamadas `__cket_apply*` e instrucciones JSON `cket.qir`).

Árbol de `#include` dentro de
\[src/cket_plugin.cpp\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp):

- L425: \[src/quantum_semantics.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc)
  (3980 líneas)
  - L1194: \[src/quantum_phase.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_phase.inc) (108
    líneas — ángulos diferidos de `phase(...)` bajo control coherente)
  - L1971: \[src/quantum_lifetimes.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_lifetimes.inc)
    (142 líneas — grafo de observación y `compute_known_storage`)
- L426: \[src/quantum_frontend.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc)
  (2347 líneas)
  - L65: \[src/quantum_heap.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_heap.inc) (677 líneas
    — `malloc`/`calloc`/`realloc`/`free` cuánticos, borrows y punteros heap)
  - L1497:
    \[src/quantum_value_program.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_value_program.inc)
    (86 líneas — programas de valor con prefijos/actualizaciones/máscaras)
  - L1498: \[src/quantum_memory.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_memory.inc) (610
    líneas — `QuantumByteRange`, `strlen`, `memcmp`/`strcmp`/`strncmp`, `memset`)
    - L404:
      \[src/quantum_string_span.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_string_span.inc)
      (64 líneas — `strspn`, `strcspn`)
    - L405:
      \[src/quantum_string_search.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_string_search.inc)
      (189 líneas — `memchr`, `strchr`, `strrchr`, `strpbrk`, `strstr`)
      - L188:
        \[src/quantum_search_positions.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_search_positions.inc)
        (173 líneas — diferencias y comparaciones de punteros de búsqueda)
      - L189:
        \[src/quantum_search_pointers.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_search_pointers.inc)
        (174 líneas — desreferencia y escritura a través de punteros de búsqueda)
    - L406:
      \[src/quantum_string_copy.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_string_copy.inc)
      (165 líneas — `strcpy`, `strncpy`)
    - L407:
      \[src/quantum_string_concat.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_string_concat.inc)
      (166 líneas — `strcat`, `strncat`)
  - L1499: \[src/quantum_byte_copy.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_byte_copy.inc)
    (91 líneas — `memcpy`/`memmove` por bytes con longitud constante o vistas parciales)
  - L1500:
    \[src/quantum_heap_transfer.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_heap_transfer.inc)
    (131 líneas — transferencia entre heap y buffers fijos/CPU)
  - L1501:
    \[src/quantum_dynamic_memory.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_dynamic_memory.inc)
    (127 líneas — `memcpy`/`memmove`/`memset` con longitud `n` en tiempo de ejecución sobre buffers
    fijos)
  - L1502: \[src/quantum_decimal.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_decimal.inc)
    (371 líneas — `atoi`, `atol`, `atoll`, `strtol`, `strtoll`, `strtoimax`)
  - L1503:
    \[src/quantum_search_writes.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_search_writes.inc)
    (95 líneas — parches enmascarados por byte en punteros de búsqueda)
  - L1505:
    \[src/quantum_memory_returns.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_memory_returns.inc)
    (97 líneas — retorno de puntero de `memcpy`/`memmove`/`memset`/`strcpy`/`strcat`)
  - L1651:
    \[src/quantum_coherent_index.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_coherent_index.inc)
    (220 líneas — indexación cuántica coherente `arr[q]` y CPU `arr[i]`)

______________________________________________________________________

### 1.2 Motor cuántico, verificador, síntesis y simulador (`build/libcket_quantum.a`)

- Cabecera pública del motor:
  \[src/engine/quantum.h\](file:///e:/Backup/GitHub/Quantum/src/engine/quantum.h) (92 líneas en Wave
  1A).
- Cabecera interna (`qc_context`, `qc_register`, `qc_gate`, `qc_heap`, `qc_mask`):
  \[src/engine/internal.h\](file:///e:/Backup/GitHub/Quantum/src/engine/internal.h) (240 líneas en
  Wave 1A; amplía `QC_MAX_PHYSICAL` de 128 a 1024 wires).
- Parser JSON liviano (`cj_doc`):
  \[src/engine/json.h\](file:///e:/Backup/GitHub/Quantum/src/engine/json.h) y
  \[src/engine/json.c\](file:///e:/Backup/GitHub/Quantum/src/engine/json.c) (212 líneas).
- Verificador estructural de IR y dominios:
  \[src/engine/verify.c\](file:///e:/Backup/GitHub/Quantum/src/engine/verify.c) (1008 líneas)
  - L303:
    \[src/engine/phase_verify.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/phase_verify.inc)
    (79 líneas — validación de `angle_expr`).
  - L481: \[`qc_verify_instruction`\](file:///e:/Backup/GitHub/Quantum/src/engine/verify.c#L481)
    (valida esquemas JSON de cada `op`).
- Simulador denso y disperso (sparse):
  \[src/engine/simulator.c\](file:///e:/Backup/GitHub/Quantum/src/engine/simulator.c) (106 líneas)
  - L7:
    \[src/engine/sparse_simulator.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/sparse_simulator.inc)
    (100 líneas — soporte disperso hasta `QC_MAX_PHYSICAL` wires: 128 en `5c5a965`, 1024 en Wave
    1A).
- Núcleo IEEE-754 de punto flotante del motor (Wave 1A, en integración Wave 1B/2):
  \[src/engine/float_arithmetic.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/float_arithmetic.inc)
  (1903 líneas — aritmética, comparaciones y conversiones reversibles `float`/`double`).
- Núcleo de ejecución y síntesis:
  \[src/engine/quantum.c\](file:///e:/Backup/GitHub/Quantum/src/engine/quantum.c) (2430 líneas)
  - L516: \[src/engine/ctype.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/ctype.inc) (336
    líneas — perfiles inmutables de `<ctype.h>` y locales).
  - L692: \[src/engine/structured.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/structured.inc)
    (402 líneas — polinomios booleanos ANF/ESOP sin tablas).
  - L694: \[src/engine/arithmetic.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/arithmetic.inc)
    (1651 líneas — síntesis aritmética reversible sin tablas, intervalos y pruebas de dominio)
    - L1087:
      \[src/engine/small_remainder.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/small_remainder.inc)
      (118 líneas)
    - L1160:
      \[src/engine/truth_views.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/truth_views.inc)
      (58 líneas)
    - L1161:
      \[src/engine/small_division.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/small_division.inc)
      (134 líneas)
    - L1162:
      \[src/engine/select_views.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/select_views.inc)
      (49 líneas)
  - L695: \[src/engine/phase.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/phase.inc) (205
    líneas — `phase_inactive`, `qc_controls_inactive`, evaluación de `angle_expr`).
  - L1004:
    \[src/engine/expression_add.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/expression_add.inc)
    (51 líneas — `qadd_expression` modular reversible).
  - L1039: \[src/engine/inplace.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/inplace.inc) (261
    líneas — actualizaciones afines in-place sin tablas).
  - L1400: \[src/engine/heap.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/heap.inc) (984 líneas
    — heap cuántico, `heap_exchange_store`, `register_patch` para `qpatch`)
    - L363:
      \[src/engine/heap_identity.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/heap_identity.inc)
      (194 líneas — pruebas de identidad en heap)
    - L984:
      \[src/engine/heap_memory.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/heap_memory.inc)
      (300 líneas — `qmemory_copy`/`qmemory_move`/`qmemory_set` sobre heap y registros)
  - L1401:
    \[src/engine/swap_rebind.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/swap_rebind.inc) (578
    líneas — `apply_swap` para `qrebind`, transporte de definiciones e inversas entre ramas CPU y
    épocas).
  - L1490:
    \[src/engine/runtime_versions.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/runtime_versions.inc)
    (49 líneas — `qversion` para historiales en bucles/ramas).
  - L1492: \[`qc_apply_token`\](file:///e:/Backup/GitHub/Quantum/src/engine/quantum.c#L1492-L2200)
    (despachador principal de todas las instrucciones JSON `op`).
  - L2429: \[src/engine/circuit.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/circuit.inc) (274
    líneas — módulos `qmodule` / `circuit()` / `inverse_circuit()`).
  - L2430: \[src/engine/binding.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/binding.inc) (450
    líneas — `qbind` y enlace seguro de parámetros CPU a expresiones JSON).

______________________________________________________________________

### 1.3 Driver CLI, Backend CLI y Runtime Nativo

- Driver compilador `build/cket`: \[src/cket.c\](file:///e:/Backup/GitHub/Quantum/src/cket.c) (332
  líneas — invoca `gcc-11 -fplugin=cket_plugin.so`, `cket_backend` y enlaza `cket_runtime.o` +
  `libcket_quantum.a`).
- Backend CLI `build/cket_backend`:
  \[src/cket_backend.c\](file:///e:/Backup/GitHub/Quantum/src/cket_backend.c) (97 líneas —
  `--verify`, `--resources`, `--qasm`, `--simulate`).
- Runtime de programas C compilados (`build/cket_runtime.o`):
  \[src/cket_runtime.c\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c) (182 líneas) +
  \[src/heap_runtime.inc\](file:///e:/Backup/GitHub/Quantum/src/heap_runtime.inc) (168 líneas).
  - Funciones puente generadas por el plugin:
    - \[`__cket_prepare`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L167),
      \[`__cket_measure`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L146),
      \[`__cket_observe`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L156)
    - \[`__cket_apply`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L86),
      \[`__cket_apply_arg`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L94),
      \[`__cket_apply_values`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L101),
      \[`__cket_apply_phase`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L127),
      \[`__cket_apply_index`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L134)
    - \[`__cket_read_byte`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L118),
      \[`__cket_controls_possible`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L78),
      \[`__cket_reject_quantum_entry`\](file:///e:/Backup/GitHub/Quantum/src/cket_runtime.c#L72)
- Cabeceras del lenguaje e interceptores C11 (`include/`):
  - \[include/quantum.h\](file:///e:/Backup/GitHub/Quantum/include/quantum.h): `quant`,
    `superposition`, `find`, `find_known`, `search`, `phase`, `phase_flip`, `fourier`,
    `inverse_fourier`, `circuit`, `inverse_circuit`, `measure`, `reset`.
  - \[include/string.h\](file:///e:/Backup/GitHub/Quantum/include/string.h),
    \[include/stdlib.h\](file:///e:/Backup/GitHub/Quantum/include/stdlib.h),
    \[include/ctype.h\](file:///e:/Backup/GitHub/Quantum/include/ctype.h): redirigen prototipos
    estándar de C11 a `include_next` con soporte de intrínsecos cuando operan sobre datos `quant`.

______________________________________________________________________

## 2. Dónde se implementa cada característica de C en el Frontend / Semantics

| Característica de C                                                                               | Ubicación en Frontend (`src/quantum_*.inc`)                                                                                                                                                                                                                                                                                                                                              | Ubicación en Semántica / Motor                                                                                                                                                                                                                                                                                                                                                     |
| ------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Variables locales `quant`, `superposition(a,b)`                                                   | \[`record_decl`\](file:///e:/Backup/GitHub/Quantum/src/cket_plugin.cpp#L324)                                                                                                                                                                                                                                                                                                             | \[`lower_quantum_decl`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L2157), `qalloc`/`qprepare`/`qcompute`                                                                                                                                                                                                                                                         |
| Arrays y `struct` fijos (`quant T arr[N]`)                                                        | \[`product_leaves`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1060), \[`product_path`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1132)                                                                                                                                                                                                               | Escalares independientes en `quantum_products`                                                                                                                                                                                                                                                                                                                                     |
| Indexación `arr[i]` (CPU) y `arr[q]` (cuántica)                                                   | \[`indexed_access`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1653), \[src/quantum_coherent_index.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_coherent_index.inc)                                                                                                                                                                                               | \[`lower_indexed_operation`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L2302), \[`indexed_action`\](file:///e:/Backup/GitHub/Quantum/src/engine/quantum.c#L1291)                                                                                                                                                                                                 |
| Punteros y borrows `quant T *p`, `const`                                                          | \[`resolve_quantum_reference`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1161), \[`readonly_quantum_access`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1668)                                                                                                                                                                                         | Validación en asignaciones, llamadas e intrínsecos                                                                                                                                                                                                                                                                                                                                 |
| Funciones `quant` (valor puro, guardas/múltiples `return`, resultado propio, `void`)              | \[`capture_quantum_function`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L650), \[`returned_expression`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L469), \[`attach_guard_fallback`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L436), \[`instantiate_quantum_call`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1023)  | Expansión higiénica por llamada con \[`clone_quantum_tree`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L139)                                                                                                                                                                                                                                                       |
| `switch` estructurado (`switch (cpu)` y `switch (quant)` coherente)                               | \[`expand_structured_switch`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L316), \[`expand_structured_switches`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L440)                                                                                                                                                                                         | Bajas a `COND_EXPR` + diagnóstico de fallthrough en \[`lower_node`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L3951)                                                                                                                                                                                                                                             |
| `return;` temprano en procedimientos `void`                                                       | \[`rewrite_procedure_returns`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L770), \[`expand_cpu_procedure_returns`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L832)                                                                                                                                                                                      | Bandera privada `__cket_procedure_active` + `procedure_cpu_branches`                                                                                                                                                                                                                                                                                                               |
| `break` y `continue` en `for`/`while`/`do`                                                        | \[`target_loop_jumps`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L850), \[`rewrite_loop_jumps`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L881), \[`expand_cpu_loop_jumps`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L927)                                                                                                           | Banderas privadas `__cket_loop_active`, `__cket_iteration_active`                                                                                                                                                                                                                                                                                                                  |
| Locales CPU privados dentro de `if (control)`                                                     | —                                                                                                                                                                                                                                                                                                                                                                                        | \[`discover_coherent_cpu_locals`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L181), \[`guarded_cpu_binding`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L329)                                                                                                                                                                                    |
| `if (q)` cuántico coherente y `if (cpu)` en `if (q)`                                              | —                                                                                                                                                                                                                                                                                                                                                                                        | \[`lower_node` (`COND_EXPR` coherente)\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L3239-L3307)                                                                                                                                                                                                                                                                   |
| `if (cpu)` clásico y unión de versiones (`qrebind`)                                               | —                                                                                                                                                                                                                                                                                                                                                                                        | \[`lower_node` (`COND_EXPR` CPU)\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L3308-L3392), \[`merge_cpu_versions`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L2038)                                                                                                                                                                             |
| Bucles `for`/`while`/`do` con condición CPU                                                       | \[`expand_static_for_block`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1795)                                                                                                                                                                                                                                                                                           | \[`lower_node` (`WHILE_STMT`/`DO_STMT`/`FOR_STMT`)\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L3393-L3449), \[`merge_cpu_paths`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L1985)                                                                                                                                                              |
| Asignación con historial, parche o limpieza                                                       | —                                                                                                                                                                                                                                                                                                                                                                                        | \[`patched_assignment`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L1738), \[`guarded_assignment`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L1837), \[`versioned_assignment`\](file:///e:/Backup/GitHub/Quantum/src/quantum_semantics.inc#L1896), \[`compute_known_storage`\](file:///e:/Backup/GitHub/Quantum/src/quantum_lifetimes.inc#L105) |
| `memcpy`, `memmove`, `memset` (longitud fija o runtime `n`)                                       | \[`expand_memory_copy`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1406), \[`expand_byte_copy`\](file:///e:/Backup/GitHub/Quantum/src/quantum_byte_copy.inc#L4), \[`expand_dynamic_memory`\](file:///e:/Backup/GitHub/Quantum/src/quantum_dynamic_memory.inc#L3), \[`expand_memory_set`\](file:///e:/Backup/GitHub/Quantum/src/quantum_memory.inc#L548)                 | \[`register_patch` (`qpatch`)\](file:///e:/Backup/GitHub/Quantum/src/engine/heap.inc#L840), \[`heap_memory_action`\](file:///e:/Backup/GitHub/Quantum/src/engine/heap_memory.inc#L126)                                                                                                                                                                                             |
| Strings (`strlen`, `strcmp`, `strncmp`, `strcpy`, `strcat`, `strchr`, `strstr`, `strspn`)         | \[src/quantum_memory.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_memory.inc), \[src/quantum_string_copy.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_string_copy.inc), \[src/quantum_string_concat.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_string_concat.inc), \[src/quantum_string_search.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_string_search.inc) | Programas de valor (`QuantumValueProgram`) y parches `qpatch`                                                                                                                                                                                                                                                                                                                      |
| `<stdlib.h>` (`abs`/`labs`/`llabs`, `div`/`ldiv`/`lldiv`, `atoi`/`atol`/`atoll`, `malloc`/`free`) | \[`expand_integer_division`\](file:///e:/Backup/GitHub/Quantum/src/quantum_frontend.inc#L1344), \[src/quantum_decimal.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_decimal.inc), \[src/quantum_heap.inc\](file:///e:/Backup/GitHub/Quantum/src/quantum_heap.inc)                                                                                                                   | \[src/engine/heap.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/heap.inc), \[src/engine/arithmetic.inc\](file:///e:/Backup/GitHub/Quantum/src/engine/arithmetic.inc)                                                                                                                                                                                                           |

______________________________________________________________________

## 3. Suite de verificación y scripts (`scripts/` y `tests/`)

- **Campaña completa normal**:
  \[scripts/verify.sh\](file:///e:/Backup/GitHub/Quantum/scripts/verify.sh)
- **Campaña completa ASan/UBSan**:
  \[scripts/sanitize-all.sh\](file:///e:/Backup/GitHub/Quantum/scripts/sanitize-all.sh)
- **Archivador e invariantes de conteos**:
  \[scripts/archive-audit.py\](file:///e:/Backup/GitHub/Quantum/scripts/archive-audit.py) (verifica
  los patrones exactos de salida en las líneas 109–151).
- **Formateador del repositorio**:
  \[scripts/format.py\](file:///e:/Backup/GitHub/Quantum/scripts/format.py) y
  \[scripts/test-format.py\](file:///e:/Backup/GitHub/Quantum/scripts/test-format.py).
- **Validadores Python de flujo/memoria/propiedad**:
  - \[scripts/validate-procedure-returns.py\](file:///e:/Backup/GitHub/Quantum/scripts/validate-procedure-returns.py):
    retornos tempranos CPU y `break`/`continue` bajo control clásico.
  - \[scripts/validate-coherent-procedure-returns.py\](file:///e:/Backup/GitHub/Quantum/scripts/validate-coherent-procedure-returns.py):
    procedimientos con `return;`, bucles `for`/`while`/`do`, `break`/`continue` y locales CPU
    privados bajo `if (control)` coherente.
  - \[scripts/validate-memory-loop-jumps.py\](file:///e:/Backup/GitHub/Quantum/scripts/validate-memory-loop-jumps.py):
    `memcpy`, `memmove`, `memset` y parches parciales en bucles con `break`/`continue`.
  - \[scripts/validate-runtime-memory.py\](file:///e:/Backup/GitHub/Quantum/scripts/validate-runtime-memory.py),
    \[scripts/validate-fixed-memory-loops.py\](file:///e:/Backup/GitHub/Quantum/scripts/validate-fixed-memory-loops.py),
    \[scripts/validate-fixed-patches.py\](file:///e:/Backup/GitHub/Quantum/scripts/validate-fixed-patches.py).
  - \[scripts/validate-ownership.py\](file:///e:/Backup/GitHub/Quantum/scripts/validate-ownership.py):
    simulador/evolucionador de referencia en Python (`evolve`, `equal_states`) usado como oráculo
    independiente.
- **Validadores C del motor (`tests/validate_*.c`)**:
  - \[tests/validate_patches.c\](file:///e:/Backup/GitHub/Quantum/tests/validate_patches.c):
    `qpatch` con máscaras clásicas/cuánticas, limpieza e inversión completa de compuertas.
  - \[tests/validate_runtime_memory.c\](file:///e:/Backup/GitHub/Quantum/tests/validate_runtime_memory.c):
    contratos preflight de `qmemory_copy`/`qmemory_move`/`qmemory_set`.
  - \[tests/validate_swap_rebind.c\](file:///e:/Backup/GitHub/Quantum/tests/validate_swap_rebind.c):
    `qrebind` y transporte de definiciones entre épocas.
  - \[tests/validate_history.c\](file:///e:/Backup/GitHub/Quantum/tests/validate_history.c),
    \[tests/validate_heap.c\](file:///e:/Backup/GitHub/Quantum/tests/validate_heap.c),
    \[tests/validate_runtime_versions.c\](file:///e:/Backup/GitHub/Quantum/tests/validate_runtime_versions.c),
    \[tests/validate_float.c\](file:///e:/Backup/GitHub/Quantum/tests/validate_float.c) (677 líneas
    — Wave 1A).
  - \[tests/test_native.c\](file:///e:/Backup/GitHub/Quantum/tests/test_native.c)
    (`build/cket_tests`): 408 pruebas nativas del compilador y motor (línea base `5c5a965`).
