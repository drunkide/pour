
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-go-1.23.6'
    TARGET_DIR = INSTALL_DIR..'/win32-go-1.23.6'
    EXTRA_PATH = { TARGET_DIR }
    EXECUTABLE = {
        _default_ = 'go',
        go = TARGET_DIR..'/go.cmd',
    }
end
