@rem Fetches (and builds if necessary) external dependencies

@rem Assume we start inside the Python source directory
call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x86

cd ..

if not exist tcltk\bin\tcl85.dll (
    cd tcl-8.5.11.0\win
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0600 OPTS=noxp INSTALLDIR=..\..\tcltk clean
    cl nmakehlp.c
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0600 OPTS=noxp INSTALLDIR=..\..\tcltk all
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0600 OPTS=noxp INSTALLDIR=..\..\tcltk install
    cd ..\..
)

if not exist tcltk\bin\tk85.dll (
    cd tk-8.5.11.0\win
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0600 OPTS=noxp INSTALLDIR=..\..\tcltk TCLDIR=..\..\tcl-8.5.11.0 clean
    cl nmakehlp.c
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0600 OPTS=noxp INSTALLDIR=..\..\tcltk TCLDIR=..\..\tcl-8.5.11.0 all
    nmake -f makefile.vc COMPILERFLAGS=-DWINVER=0x0600 OPTS=noxp INSTALLDIR=..\..\tcltk TCLDIR=..\..\tcl-8.5.11.0 install
    cd ..\..
)

