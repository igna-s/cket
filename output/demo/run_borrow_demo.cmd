@echo off
setlocal
wsl -d Ubuntu2204 --exec mkdir -p "/tmp/cket-borrow-demo-f0305f42f48c"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-borrow-demo-f0305f42f48c" --exec tar -xzf - < "%~dp0cket-borrow-demo-linux-x86_64.tar.gz"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-borrow-demo-f0305f42f48c" --exec sha256sum -c SHA256SUMS
if errorlevel 1 exit /b 1
for %%S in (1 7 13) do (
  wsl -d Ubuntu2204 --cd "/tmp/cket-borrow-demo-f0305f42f48c" --exec env CKET_SIMULATOR=sparse CKET_SEED=%%S ./bin/borrow_cleanup
  if errorlevel 1 exit /b 1
)
exit /b 0
