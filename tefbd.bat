@echo off
set PATH=%~dp0\PCBuild\amd64:%PATH%
set PYTHONPATH=%~dp0\Lib
PCbuild\amd64\python_d -m ctk.cli tefb techempower_frameworks_benchmark %*

rem @python_d -m ctk.cli px px %*
