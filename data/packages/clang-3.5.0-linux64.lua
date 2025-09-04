
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-clang350-linux64'
    TARGET_DIR = INSTALL_DIR..'/win32-clang-3.5.0-linux64'
    EXTRA_PATH = { TARGET_DIR..'/bin' }
    EXECUTABLE = {
        _default_ = 'clang',
        clang = TARGET_DIR..'/bin/clang.exe',
    }
end
