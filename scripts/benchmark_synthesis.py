#!/usr/bin/env python3
"""Compile widths 4/8/12/16 with tables disabled; publish growth and workspace."""
import csv
import json
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def expression(op, width):
    def leaf(id):
        return {"op": "reg", "id": id, "bits": width, "signed": False}
    divisor = {"op": "bor", "bits": width, "signed": False,
               "args": [leaf(1), {"op": "const", "bits": width, "signed": False, "value": 1}]}
    return {"op": op, "bits": 1 if op == "lt" else width, "signed": False,
            "args": [leaf(0), divisor if op in ("div", "mod") else leaf(1)]}


def main():
    rows = []
    with tempfile.TemporaryDirectory(prefix="cket-growth-") as folder:
        path = pathlib.Path(folder) / "program.json"
        for op in ("add", "sub", "mul", "div", "mod", "lt"):
            previous = None
            for width in (4, 8, 12, 16):
                instructions = []
                for id in (0, 1):
                    instructions += [{"op": "qalloc", "id": id, "width": width, "signed": False},
                                     {"op": "qprepare", "id": id, "lower": 0, "upper": (1 << width) - 1}]
                instructions += [{"op": "qalloc", "id": 2, "width": 1 if op == "lt" else width, "signed": False},
                                 {"op": "qcompute", "id": 2, "expr": expression(op, width)}]
                path.write_text(json.dumps({"format": "cket.qir", "version": 1, "instructions": instructions}))
                output = subprocess.check_output([str(ROOT / "build/cket_backend"), "--resources", str(path), "--no-tables"], cwd=ROOT, text=True)
                stats = dict(re.findall(r"^([A-Za-z ]+):\s*(\d+)$", output, re.M))
                gates = sum(int(n) for name, n in re.findall(r"^(H|X|CNOT|Toffoli|MCX|PHASE|GLOBAL_PHASE)\s+(\d+)$", output, re.M))
                ratio = gates / previous if previous else 0
                if previous:
                    # Logical increment and MCX costs yield at most cubic growth.
                    assert ratio <= 9, (op, width, ratio)
                assert int(stats["Ancillas"]) <= 4 * width + 2, (op, width, stats)
                rows.append({"operator": op, "width": width, "relevant_bits": 2 * width,
                             "gates": gates, "ancillas": int(stats["Ancillas"]), "peak_qubits": int(stats["Peak qubits"]),
                             "depth": int(stats["Depth"]), "gate_ratio": round(ratio, 4)})
                previous = gates
                print(f"PASS growth {op} width={width}: gates={gates}, ancillas={stats['Ancillas']}", flush=True)
    target = ROOT / "build/synthesis-growth.csv"
    with target.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(f"{len(rows)} circuits compiled without tables; {target}")


if __name__ == "__main__":
    main()
