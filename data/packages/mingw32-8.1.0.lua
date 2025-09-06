
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-mingw810-32'
    TARGET_DIR = INSTALL_DIR..'/win32-mingw810-32'
    EXTRA_PATH = { TARGET_DIR..'/bin' }
    EXECUTABLE = {
        _default_ = 'gcc',
        ['ar'     ] = TARGET_DIR..'/bin/ar.exe',
        ['ranlib' ] = TARGET_DIR..'/bin/ranlib.exe',
        ['gcc'    ] = TARGET_DIR..'/bin/gcc.exe',
        ['g++'    ] = TARGET_DIR..'/bin/g++.exe',
        ['dlltool'] = TARGET_DIR..'/bin/dlltool.exe',
    }
end
