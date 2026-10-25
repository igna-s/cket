@echo off
setlocal
wsl -d Ubuntu2204 --exec mkdir -p "/tmp/cket-heap-demo-1a48c313e7cd"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-heap-demo-1a48c313e7cd" --exec tar -xzf - < "%~dp0cket-heap-demo-linux-x86_64.tar.gz"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "/tmp/cket-heap-demo-1a48c313e7cd" --exec sha256sum -c SHA256SUMS
if errorlevel 1 exit /b 1
for %%S in (1 7 13 29) do (
  wsl -d Ubuntu2204 --cd "/tmp/cket-heap-demo-1a48c313e7cd" --exec env CKET_SIMULATOR=sparse CKET_SEED=%%S ./bin/heap_coherence
  if errorlevel 1 exit /b 1
)
exit /b 0
