@echo off
set "CKET_SDK_ROOT=/tmp/cket-sdk-684dc85ef82e"
wsl -d Ubuntu2204 --exec mkdir -p "%CKET_SDK_ROOT%"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "%CKET_SDK_ROOT%" --exec tar -xzf - < "%~dp0cket-sdk-linux-x86_64.tar.gz"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "%CKET_SDK_ROOT%" --exec sha256sum -c --status SHA256SUMS
if errorlevel 1 exit /b 1
exit /b 0
