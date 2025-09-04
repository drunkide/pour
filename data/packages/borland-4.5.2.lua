
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-borland452'
    TARGET_DIR = INSTALL_DIR..'/win32-borland452'
    EXTRA_DEPS = { 'gnu' }
    EXTRA_PATH = { TARGET_DIR..'/bin' }
    EXECUTABLE = {
        _default_ = 'bcc32',
        bcc32 = TARGET_DIR..'/bcc32.cmd',
        tlib = TARGET_DIR..'/tlib.cmd',
    }
end
