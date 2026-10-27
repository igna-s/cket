@echo off
call "%~dp0compile_and_run.cmd" "%~dp0example_returns.c"
exit /b %errorlevel%
