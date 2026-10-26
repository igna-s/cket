@echo off
setlocal
wsl -d Ubuntu2204 --exec mkdir -p "/tmp/cket-lazy-demo-23a089c7020f"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-lazy-demo-23a089c7020f" --exec tar -xzf - < "%~dp0cket-lazy-demo-linux-x86_64.tar.gz"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-lazy-demo-23a089c7020f" --exec sha256sum -c SHA256SUMS
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-lazy-demo-23a089c7020f" --exec env CKET_SIMULATOR=sparse CKET_SEED=7 ./bin/lazy_values_basis
if errorlevel 1 exit /b 1
for %%S in (1 7 13 29) do (
  wsl -d Ubuntu2204 --cd "/tmp/cket-lazy-demo-23a089c7020f" --exec env CKET_SIMULATOR=sparse CKET_SEED=%%S ./bin/lazy_values
  if errorlevel 1 exit /b 1
)
exit /b 0
