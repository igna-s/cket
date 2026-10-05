#!/usr/bin/env python3
"""Load unmodified exported QASM in Qiskit; compare amplitudes and outcomes."""
import json
import math
import os
import pathlib
import subprocess
import tempfile

import numpy as np
from qiskit import qasm3, transpile
from qiskit.quantum_info import Statevector
from qiskit_aer import AerSimulator

ROOT = pathlib.Path(__file__).resolve().parents[1]
CASES = {
    "entangled arithmetic": "quant uint8_t x=superposition(0,3);quant uint8_t y=x%2;measure(y);measure(x);",
    "relative phase and cleanup": "quant uint8_t x=superposition(0,7);if(x==3)phase(.7);measure(x);",
    "controlled global phase": "quant bool c=superposition(0,1);if(c)phase(.3);measure(c);",
    "open controlled Fourier": "quant bool c=superposition(0,1);quant uint8_t x=1;if(c)phase(.4);else fourier(x);measure(x);measure(c);",
    "Fourier inverse interference": "quant uint8_t x=5;fourier(x);if(x==3)phase(.8);inverse_fourier(x);measure(x);",
    "Grover complete circuit": "quant uint8_t x=superposition(0,15);quant uint8_t y=find_known(x,x%4==1,4);measure(y);",
    "nested open controls": "quant bool a=superposition(0,1),b=superposition(0,1);quant uint8_t x=0;if(a){if(b)x+=1;else x+=2;}else x+=3;measure(x);measure(b);measure(a);",
    "independent controlled module": f'quant bool c=superposition(0,1);quant uint8_t x=7;if(c)circuit("{ROOT}/examples/increment.qir.json",x);measure(x);measure(c);',
}
CHANNELS = {
    "intermediate entangled measurement": "quant bool a=superposition(0,1),b=false;if(a)b^=true;measure(a);fourier(b);if(b)phase(.4);inverse_fourier(b);measure(b);",
    "reset of an entangled input": "quant bool a=superposition(0,1),b=false;if(a)b^=true;reset(a);fourier(b);measure(b);measure(a);",
    "measurement then reuse": "quant bool a=superposition(0,1);measure(a);fourier(a);if(a)phase(.8);inverse_fourier(a);measure(a);",
}


def run(*args):
    return subprocess.check_output([str(a) for a in args], cwd=ROOT, text=True)


def check(name, body, folder):
    source = folder / "program.c"
    ir = folder / "program.json"
    source.write_text("#include <quantum.h>\nint main(void){" + body + "return 0;}\n")
    run(ROOT / "build/cket", source, "--emit-ir", "-o", ir)
    exported = run(ROOT / "build/cket_backend", "--emit-qasm", ir)
    # No rewriting or private parser: the independent importer reads the artifact.
    circuit = qasm3.loads(exported)
    native = json.loads(run(ROOT / "build/cket_backend", "--state", ir))
    expected = np.zeros(1 << native["qubits"], dtype=complex)
    for amplitude in native["amplitudes"]:
        expected[amplitude["index"]] = amplitude["real"] + 1j * amplitude["imag"]
    actual = Statevector.from_instruction(circuit.remove_final_measurements(inplace=False)).data
    overlap = np.vdot(expected, actual)
    assert abs(abs(overlap) - 1) < 1e-10, (name, "state overlap", overlap)
    assert np.max(np.abs(actual - expected * overlap / abs(overlap))) < 1e-10, name

    measured = []
    for instruction in circuit.data:
        if instruction.operation.name == "measure":
            measured.append((circuit.find_bit(instruction.qubits[0]).index,
                             circuit.find_bit(instruction.clbits[0]).index))
    probabilities = {}
    for basis, amplitude in enumerate(expected):
        if abs(amplitude) < 1e-12:
            continue
        bits = sum(((basis >> wire) & 1) << bit for wire, bit in measured)
        probabilities[bits] = probabilities.get(bits, 0) + abs(amplitude) ** 2
    shots = 8192
    simulator = AerSimulator(seed_simulator=913)
    counts = simulator.run(transpile(circuit, simulator, seed_transpiler=913), shots=shots).result().get_counts()
    observed = {int(bits.replace(" ", ""), 2): count / shots for bits, count in counts.items()}
    # Hoeffding + union bound, family-wise failure <= 1e-6 across this suite.
    tolerance = math.sqrt(math.log(2 * (len(CASES) + len(CHANNELS)) * max(1, len(probabilities)) / 1e-6) / (2 * shots))
    for value in observed.keys() | probabilities.keys():
        assert abs(observed.get(value, 0) - probabilities.get(value, 0)) < tolerance, (name, "measurement", value)
    print(f"PASS independent QASM: {name} ({circuit.num_qubits} qubits)", flush=True)


def check_channel(name, body, folder):
    source, ir = folder / "channel.c", folder / "channel.json"
    source.write_text("#include <quantum.h>\nint main(void){" + body + "return 0;}\n")
    run(ROOT / "build/cket", source, "--emit-ir", "-o", ir)
    circuit = qasm3.loads(run(ROOT / "build/cket_backend", "--emit-qasm", ir))
    shots = 8192
    native = {int(bits): count / shots for bits, count in json.loads(run(ROOT / "build/qasm_samples", ir, shots)).items()}
    simulator = AerSimulator(seed_simulator=913)
    counts = simulator.run(transpile(circuit, simulator, seed_transpiler=913), shots=shots).result().get_counts()
    independent = {int(bits.replace(" ", ""), 2): count / shots for bits, count in counts.items()}
    # Two empirical distributions: each deviates by epsilon from its true law.
    # Bound all possible transcripts, including outcomes absent from one sample.
    tolerance = 2 * math.sqrt(math.log(4 * (len(CASES) + len(CHANNELS)) * (1 << circuit.num_clbits) / 1e-6) / (2 * shots))
    for value in native.keys() | independent.keys():
        assert abs(native.get(value, 0) - independent.get(value, 0)) < tolerance, (name, "channel transcript", value)
    print(f"PASS independent QASM channel: {name} ({circuit.num_qubits} qubits)", flush=True)


if __name__ == "__main__":
    run(os.environ.get("CC", "gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-Inative",
        "scripts/qasm_samples.c", "build/libcket_quantum.a", "-lm", "-o", "build/qasm_samples")
    with tempfile.TemporaryDirectory(prefix="cket-qasm-") as temp:
        for name, body in CASES.items():
            check(name, body, pathlib.Path(temp))
        for name, body in CHANNELS.items():
            check_channel(name, body, pathlib.Path(temp))
    print(f"{len(CASES) + len(CHANNELS)} exported circuits independently validated")
