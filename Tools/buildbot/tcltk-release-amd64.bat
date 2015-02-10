@rem Fetches (and builds if necessary) external dependencies

@rem Assume we start inside the Python source directory
call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64

cd ..

if not exist tcltk64\bin\tcl85.dll (
    cd tcl-8.5.11.0\win
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0500 OPTS=noxp MACHINE=AMD64 INSTALLDIR=..\..\tcltk64 clean
    cl nmakehlp.c
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0500 OPTS=noxp MACHINE=AMD64 INSTALLDIR=..\..\tcltk64 all
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0500 OPTS=noxp MACHINE=AMD64 INSTALLDIR=..\..\tcltk64 install
    cd ..\..
)

if not exist tcltk64\bin\tk85.dll (
    cd tk-8.5.11.0\win
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0500 OPTS=noxp MACHINE=AMD64 INSTALLDIR=..\..\tcltk64 TCLDIR=..\..\tcl-8.5.11.0 clean
    cl nmakehlp.c
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0500 OPTS=noxp MACHINE=AMD64 INSTALLDIR=..\..\tcltk64 TCLDIR=..\..\tcl-8.5.11.0 all
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0500 OPTS=noxp MACHINE=AMD64 INSTALLDIR=..\..\tcltk64 TCLDIR=..\..\tcl-8.5.11.0 install
    cd ..\..
)

