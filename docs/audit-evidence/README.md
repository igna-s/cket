# Evidence for the current C11 roadmap baseline

`verify.log` records the complete normal and ASan/UBSan verification campaign. `results.json`
identifies its base commit, modified working tree, GCC version, seed, counts, source hashes and
artifact hashes. `synthesis-growth.csv` contains 24 resource rows. The tested source state is
identified by its hashes; the base commit alone does not identify the tested modifications.

Reproduce from these sources on the declared GCC 11.4.0 Linux/WSL target:

```bash
export CC=gcc-11 CXX=g++-11
export CKET_FUZZ_CASES=128 CKET_FUZZ_SEED=20261012
mkdir -p build/audit
bash scripts/verify.sh >build/audit/verify-final.log 2>&1
python3 scripts/archive-audit.py --log build/audit/verify-final.log
```

Source hashes are captured when archiving; the script does not monitor source changes during
compilation. Run verification and archival without intervening compiler/runtime/test changes. The
archive script checks completed stage results before copying them. Preserve an existing archive
before rerunning it if comparing campaigns. This is finite evidence for the implemented subset and
thirty-four state/three channel comparisons with QuEST, not proof of all C11/library semantics or
hardware execution. The historical paper evidence remains in `docs/paper/evidence/` and belongs to a
different fixed revision and campaign.

The current campaign includes 55 shift checks, 97 pure-expression checks, 271 in-place checks and
414 memory-copy checks, 990 integer-division checks, 399 CPU-binding checks, 348 history checks and
18 validation-runner fault-injection checks in each configuration. Wide circuit-generation checks
are not counted as complete state validation. The source manifest also records the formatting
configuration, optional tool versions and CI workflow used for this source tree.
