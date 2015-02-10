@echo off
rem set PATH=%~dp0:%PATH%
rem set PYTHONPATH=%~dp0\Lib
set PATH=C:\Python33:%PATH%
set PYTHONPATH=
cd %~dp0\website
C:\Python33\python -m http.server

rem @python_d -m ctk.cli px px %*
