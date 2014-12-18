@echo off
set PATH=%~dp0\PCBuild\amd64:%PATH%
set PYTHONPATH=%~dp0\Lib
PCbuild\amd64\python -m ctk.cli px px %*

rem @python_d -m ctk.cli px px %*
