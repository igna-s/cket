# ABI nativa de circuitos

Un módulo es un documento `cket.qir`, versión 1, independiente del llamador. Sus parámetros se declaran al principio mediante `qborrow`, antes de cualquier operación del cuerpo. Identidad local, anchura y signo definen cada parámetro; el orden de declaración determina el orden de argumentos.

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

Los parámetros se analizan como estados desconocidos, incluso si el llamador prepara una base. El cuerpo puede asignar trabajo privado, pero debe descomputarlo/liberarlo y no dejar certificados pendientes. Medición, reset, búsqueda y feedback CPU se rechazan. Los parámetros permanecen prestados al terminar.

La API de [native/quantum.h](../native/quantum.h) compila una vez con `qc_circuit_compile`, aplica a identidades del contexto con `qc_circuit_apply` y destruye con `qc_circuit_destroy`. Se remapean cables y se verifican disponibilidad, anchura, signo, alias y dependencias de entradas modificadas. Es una ABI de representación de registros, no una ABI completa de tipos nominales de C.

La inversa invierte orden y ángulos. Los controles externos se acumulan sobre todas las puertas, incluidas las fases globales; no se elimina su efecto relativo. El trabajo privado comienza/termina en cero, también bajo control en superposición. Las mutaciones invalidan conservadoramente versiones y rangos del llamador.

Desde C:

```c
quant bool flag = superposition(0, 1);
quant uint8_t x = 7;
if (flag) circuit("increment.qir.json", x);
if (flag) inverse_circuit("increment.qir.json", x);
measure(x);
measure(flag);
```

El archivo se resuelve respecto del archivo C que contiene la llamada y se incorpora durante la compilación. El runtime lo valida/compila en la primera llamada y reutiliza el plan; no vuelve a abrirlo. [examples/circuit_module.c](../examples/circuit_module.c) y [examples/increment.qir.json](../examples/increment.qir.json) forman un ejemplo completo.

Los argumentos C son préstamos mutables conservadores sin inversas pendientes. Retornar propietarios nuevos, transportar productos, enlazar funciones C `quant` entre unidades y emitir una tabla definitiva de puertas durante la compilación siguen pendientes.
