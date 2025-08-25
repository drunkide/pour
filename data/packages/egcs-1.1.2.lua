
if WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/egcs-1.1.2'
    TARGET_DIR = INSTALL_DIR..'/egcs-1.1.2'
    EXTRA_PATH = { TARGET_DIR }
    EXECUTABLE = {
        _default_ = 'gcc',
        ['ar'    ] = TARGET_DIR..'/ar.cmd',
        ['ranlib'] = TARGET_DIR..'/ranlib.cmd',
        ['gcc'   ] = TARGET_DIR..'/gcc.cmd',
        ['g++'   ] = TARGET_DIR..'/g++.cmd',
    }
end
