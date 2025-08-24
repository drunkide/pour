
if WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-ninja'
    TARGET_DIR = TOOLS_DIR..'/win32-ninja'
    EXTRA_PATH = { TARGET_DIR }
    EXECUTABLE = {
        _default_ = 'ninja',
        ninja = TARGET_DIR..'/ninja.exe',
    }
end
