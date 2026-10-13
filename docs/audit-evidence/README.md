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
python3 scripts/archive-audit.py --freeze build/audit/current-sources.json
bash scripts/verify.sh >build/audit/verify-final.log 2>&1
python3 scripts/archive-audit.py --log build/audit/verify-final.log \
    --sources build/audit/current-sources.json
```

The source snapshot is created before verification and compared with the hashes captured when
archiving. These endpoint checks do not continuously monitor compilation. The snapshot command
refuses overwriting an existing file, and an archival hash mismatch preserves the old archive. Run
verification and archival without intervening compiler/runtime/test changes. The archive script
checks completed stage results before copying them. Preserve an existing archive before rerunning it
if comparing campaigns. This is finite evidence for the implemented subset and forty-five state/six
channel comparisons with QuEST, not proof of all C11/library semantics or hardware execution. The
historical paper evidence remains in `docs/paper/evidence/` and belongs to a different fixed
revision and campaign.

The current campaign includes 55 shift checks, 97 pure-expression checks, 271 in-place checks and
414 memory-copy checks, 990 integer-division checks, 399 CPU-binding checks, 2332 history checks,
635 C-locale character checks and 18 validation-runner fault-injection checks in each configuration.
Wide circuit-generation checks are not counted as complete state validation. The source manifest
also records the formatting configuration, optional tool versions and CI workflow used for this
source tree.

The CPU-join coverage includes 46 ownership contracts, 55 C programs, 120 native paths and 240
complete selected-trace states; the eight-bit cases use known basis inputs. The full campaign ran
with 115 source hashes frozen before execution and checked unchanged afterward. The archived hashes
identify those same tested bytes. Three additional selected CPU traces were checked independently;
unresolved CPU templates remain distinct from verified flat circuits.

The ordered scalar effect campaign adds 220 checks: 120 native old/new results, 60 complete phased
states and 40 diagnostics at five GCC levels. Three additional QuEST state fixtures cover postfix,
prefix and comma results. Mixed CPU effects, general memory/float observations and full C11 coverage
remain unfinished.

The integer observation campaign adds 730 checks: 260 direct channel contracts, 120 native C
executions, 320 conditional states of compiled C and 30 diagnostics. Three additional QuEST channels
compare predicate interference and postfix ownership. The formatter has three contract-preservation
regressions. The complete C11/library objective remains open.

The selected lazy effect campaign adds 573 checks: 320 C reference executions, 65 integrated
programs, 160 complete phased states, 20 diagnostics and eight fixed-predicate contracts. Three more
table-free QuEST states contrast conditional updates, nested short circuits and condition
overwrites. The full C11/library goal remains unfinished.

The C-locale character campaign adds 635 checks: 560 complete phased states over all byte values, 50
combined programs, 15 diagnostics and ten classical GCC differentials. Two more table-free QuEST
fixtures contrast classifications and a conversion function with a memory borrow. This is partial
coverage of the fourteen interfaces; general locale snapshots and full C11 remain open.
