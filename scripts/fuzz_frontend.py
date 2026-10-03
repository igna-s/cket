#!/usr/bin/env python3
"""Reproducible generated C programs, checked against GCC CPU expressions."""
import argparse
import json
import os
import pathlib
import random
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
LEVELS = ("-O0", "-O1", "-O2", "-O3", "-Os")


def expression(rng, depth):
    if not depth or rng.randrange(5) == 0:
        return rng.choice(("x", "(unsigned)x", str(rng.randrange(1, 16))))
    left = expression(rng, depth - 1)
    op = rng.choice(("+", "-", "*", "^", "&", "|", "%", "/", "<<", ">>", "<", "=="))
    if op in ("/", "%"):
        right = str(rng.randrange(1, 16))
    elif op in ("<<", ">>"):
        left = f"(unsigned)({left})"
        right = str(rng.randrange(4))
    else:
        right = expression(rng, depth - 1)
    # Unsigned conversions make arithmetic defined over the generated domain.
    return f"((unsigned)({left}) {op} (unsigned)({right}))"


def program(expr):
    lines = ["#include <quantum.h>", "int main(void){int bad=0;"]
    for i in (0, 1, 3, 7, 15, 127, 255):
        qexpr = expr.replace("x", f"q{i}")
        reference = expr.replace("x", str(i))
        lines.append(f"quant uint8_t q{i}={i};quant uint8_t r{i}={qexpr};bad|=measure(r{i})!=(uint8_t)({reference});reset(r{i});reset(q{i});")
    lines += ["quant uint8_t a=1,b=2;int n=1;", "a+=++n; a+=measure(b);bad|=n!=2 || measure(a)!=5;reset(a);reset(b);", "return bad;}"]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=20261011)
    parser.add_argument("--cases", type=int, default=24)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    # Exercise promotions, narrowing and evaluation of guarded undefined arithmetic.
    expressions = ["x?255/x:7", "x && (7/x)", "!x || (7/x)", "(uint8_t)(x*x)", "((int8_t)x)<0"]
    expressions += [expression(rng, 3) for _ in range(args.cases)]
    with tempfile.TemporaryDirectory(prefix="cket-frontend-") as temp:
        source, binary = pathlib.Path(temp) / "program.c", pathlib.Path(temp) / "program"
        for index, expr in enumerate(expressions):
            source.write_text(program(expr))
            for level in LEVELS:
                command = [str(ROOT / "build/cket"), str(source), level, "--run", "--seed", "7", "-o", str(binary)]
                result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, env=os.environ.copy(), timeout=30)
                if result.returncode:
                    output = ROOT / "build/frontend-failure.c"
                    output.write_text(source.read_text())
                    (ROOT / "build/frontend-failure.json").write_text(json.dumps({"seed": args.seed, "case": index, "expression": expr, "optimization": level, "stderr": result.stderr}, indent=2))
                    raise RuntimeError(f"frontend differential failed: seed={args.seed}, case={index}, {level}; reproducer={output}\n{result.stderr}")
            print(f"PASS frontend {index}: all {len(LEVELS)} optimization levels", flush=True)
    print(f"{len(expressions)*len(LEVELS)} generated frontend compilations passed; seed={args.seed}")


if __name__ == "__main__":
    main()
