
if WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-mingw440-32'
    TARGET_DIR = TOOLS_DIR..'/win32-mingw440-32'
    EXTRA_PATH = { TARGET_DIR..'/bin' }
    EXECUTABLE = {
        _default_ = 'gcc',
        ['ar' ] = TARGET_DIR..'/bin/ar.exe',
        ['gcc'] = TARGET_DIR..'/bin/gcc.exe',
        ['g++'] = TARGET_DIR..'/bin/g++.exe',
    }
end
