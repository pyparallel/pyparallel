@echo off
set PATH=%~dp0\PCBuild\amd64:%PATH%
set PYTHONPATH=%~dp0\Lib
%~dp0\PCbuild\amd64\python_d %*
