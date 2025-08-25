
if WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-mingw32-make'
    TARGET_DIR = TOOLS_DIR..'/win32-mingw32-make'
    EXTRA_PATH = { TARGET_DIR }
    EXECUTABLE = {
        _default_ = 'make',
        make = TARGET_DIR..'/mingw32-make.exe',
    }
end
