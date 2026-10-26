@echo off
setlocal
if "%~1"=="" (
  echo Usage: compile_and_run.cmd program.c
  exit /b 2
)
if not exist "%~1" exit /b 2
if not "%~2"=="" exit /b 2
call "%~dp0extract_sdk.cmd"
if errorlevel 1 exit /b 1
wsl -d Ubuntu2204 --cd "%CKET_SDK_ROOT%" --exec bash compile-user.sh < "%~1"
exit /b %errorlevel%
