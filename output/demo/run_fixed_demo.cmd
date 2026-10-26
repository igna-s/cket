@echo off
setlocal
wsl -d Ubuntu2204 --exec mkdir -p "/tmp/cket-fixed-demo-af94a7f30c9a"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-fixed-demo-af94a7f30c9a" --exec tar -xzf - < "%~dp0cket-fixed-demo-linux-x86_64.tar.gz"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-fixed-demo-af94a7f30c9a" --exec sha256sum -c SHA256SUMS
if errorlevel 1 exit /b 1
for %%N in (0 1) do (
  for %%S in (1 7 13 29) do (
    wsl -d Ubuntu2204 --cd "/tmp/cket-fixed-demo-af94a7f30c9a" --exec env CKET_SIMULATOR=sparse CKET_SEED=%%S ./bin/fixed_coherence %%N
    if errorlevel 1 exit /b 1
  )
)
exit /b 0
