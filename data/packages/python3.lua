
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-python3'
    TARGET_DIR = INSTALL_DIR..'/win32-python3'
    EXTRA_PATH = { TARGET_DIR }
    EXECUTABLE = {
        _default_ = 'python',
        python = TARGET_DIR..'/python.exe',
    }
end
