@echo off
set PATH=%~dp0\PCBuild\x64-pgi:%PATH%
set PYTHONPATH=%~dp0\Lib
call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64
%~dp0\PCbuild\x64-pgi\python.exe %*
