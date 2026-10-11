# Native circuit ABI

A module is a `cket.qir` version 1 document independent of its caller. Parameters are declared at the beginning with `qborrow`, before any body operation. Local identity, width, and signedness define each parameter; declaration order determines argument order.

```json
{
  "format": "cket.qir",
  "version": 1,
  "instructions": [
    {"op": "qborrow", "id": 0, "width": 8, "signed": false},
    {"op": "qadd_inplace", "id": 0, "value": 1},
    {"op": "qphase", "angle": 0.3}
  ]
}
```

Parameters are analyzed as unknown states, even if the caller prepares a basis state. The body may allocate private workspace, but must uncompute and release it without leaving pending certificates. Measurement, reset, search, and CPU feedback are rejected. Parameters remain borrowed when the module finishes.

The API in [native/quantum.h](../native/quantum.h) compiles once with `qc_circuit_compile`, applies to context identities with `qc_circuit_apply`, and destroys the module with `qc_circuit_destroy`. Wires are remapped; availability, width, signedness, aliasing, and dependencies on modified inputs are checked. This is a register representation ABI, rather than a complete ABI for nominal C types.

The inverse reverses gate order and angles. External controls accumulate on every gate, including global phases, preserving their relative effect. Private workspace starts and ends in zero, including under a control in superposition. Mutations conservatively invalidate the caller's versions and ranges.

From C:

```c
quant bool flag = superposition(0, 1);
quant uint8_t x = 7;
if (flag) circuit("increment.qir.json", x);
if (flag) inverse_circuit("increment.qir.json", x);
measure(x);
measure(flag);
```

The file is resolved relative to the C file containing the call and embedded during compilation. The runtime validates and compiles it on the first call, then reuses the plan without reopening the file. [examples/circuit_module.c](../examples/circuit_module.c) and [examples/increment.qir.json](../examples/increment.qir.json) provide a complete example.

C arguments are conservative mutable borrows without pending inverses. Returning new owners, passing products, linking C `quant` functions across translation units, and emitting a final gate table during compilation remain pending.
