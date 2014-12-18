@echo off
set PATH=%~dp0:%PATH%
set PYTHONPATH=%~dp0\Lib
cd %~dp0\website
..\python -m http.server

rem @python_d -m ctk.cli px px %*
