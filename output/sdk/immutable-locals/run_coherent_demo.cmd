@echo off
call "%~dp0compile_and_run.cmd" "%~dp0example_coherent.c"
exit /b %errorlevel%
