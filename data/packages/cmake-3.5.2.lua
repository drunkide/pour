
if WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-cmake-3.5.2'
    TARGET_DIR = TOOLS_DIR..'/win32-cmake-3.5.2'
    EXTRA_PATH = { TARGET_DIR..'/bin' }
    EXECUTABLE = {
        _default_ = 'cmake',
        cmake = TARGET_DIR..'/bin/cmake.exe',
        cpack = TARGET_DIR..'/bin/cpack.exe',
        ctest = TARGET_DIR..'/bin/ctest.exe',
    }
end
