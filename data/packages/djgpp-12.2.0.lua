
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-djgpp-12.2.0'
    TARGET_DIR = INSTALL_DIR..'/win32-djgpp-12.2.0'
    EXTRA_PATH = { TARGET_DIR..'/bin' }
    EXECUTABLE = {
        _default_ = 'gcc',
        ['ar'    ] = TARGET_DIR..'/bin/i586-pc-msdosdjgpp-ar.exe',
        ['ranlib'] = TARGET_DIR..'/bin/i586-pc-msdosdjgpp-ranlib.exe',
        ['gcc'   ] = TARGET_DIR..'/bin/i586-pc-msdosdjgpp-gcc.exe',
        ['g++'   ] = TARGET_DIR..'/bin/i586-pc-msdosdjgpp-g++.exe',
    }
end
