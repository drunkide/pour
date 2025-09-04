
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/win32-watcom10'
    TARGET_DIR = INSTALL_DIR..'/win32-watcom10'
    EXECUTABLE = {
        _default_ = 'wcc386',
        wcc386 = TARGET_DIR..'/wcc386.cmd',
        wlib = TARGET_DIR..'/wlib.cmd',
        wlink = TARGET_DIR..'/wlink.cmd',
    }
end
