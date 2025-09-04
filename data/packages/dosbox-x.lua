
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-dosbox-x-20241204'
    TARGET_DIR = INSTALL_DIR..'/win32-dosbox'
    EXECUTABLE = {
        _default_ = 'dosbox',
        dosbox = TARGET_DIR..'/dosbox-x.exe',
    }
end
