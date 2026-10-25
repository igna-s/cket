@echo off
setlocal
wsl --exec mkdir -p "/tmp/cket-memory-demo-1438154403a1"
if errorlevel 1 exit /b 1
wsl --cd "/tmp/cket-memory-demo-1438154403a1" --exec tar -xzf - < "%~dp0cket-memory-demo-linux-x86_64.tar.gz"
if errorlevel 1 exit /b 1
for %%N in (0 1 2) do (
  wsl --cd "/tmp/cket-memory-demo-1438154403a1" --exec env CKET_SIMULATOR=sparse CKET_SEED=7 ./bin/buffer_lengths %%N
  if errorlevel 1 exit /b 1
)
exit /b 0
