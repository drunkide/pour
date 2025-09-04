
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-gnu'
    TARGET_DIR = INSTALL_DIR..'/win32-gnu'
    EXTRA_PATH = { TARGET_DIR }
    EXECUTABLE = {
        _default_ = 'diff',
        cp = TARGET_DIR..'/cp.exe',
        diff = TARGET_DIR..'/diff.exe',
        grep = TARGET_DIR..'/grep.exe',
        mkdir = TARGET_DIR..'/mkdir.exe',
        rm = TARGET_DIR..'/rm.exe',
        sed = TARGET_DIR..'/sed.exe',
    }
end
