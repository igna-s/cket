@echo off
setlocal
call "%~dp0extract_sdk.cmd"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "%CKET_SDK_ROOT%" --exec bash run-demo.sh
exit /b %errorlevel%
