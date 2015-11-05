#!C:\Users\Trent\Home\src\pyparallel\PCbuild\amd64\python.exe
# EASY-INSTALL-ENTRY-SCRIPT: 'Cython==0.23.4','console_scripts','cythonize'

if __name__ == '__main__':
    from Cython.Compiler.Main import main
    main()

if __name__ == '__main__x':
    __requires__ = 'Cython==0.23.4'
    from pkg_resources import load_entry_point
    import pdb
    dbg = pdb.Pdb()
    dbg.set_trace()
    entry_point = load_entry_point(
        'Cython==0.23.4',
        'console_scripts',
        'cython'
    )

    retval = entry_point()
    import sys
    sys.exit(retval)
