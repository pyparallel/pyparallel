@echo off
set PATH=%~dp0\PCBuild\x64-pgo:%PATH%
set PYTHONPATH=%~dp0\Lib
PCbuild\x64-pgo\python.exe %*
