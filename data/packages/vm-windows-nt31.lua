
SOURCE_URL = 'git@github.com:thirdpartystuff/vm-windows-nt31'
TARGET_DIR = INSTALL_DIR..'/vm-windows-nt31'
INVOKE_LUA = TARGET_DIR..'/config.lua'

if HOST_WINDOWS then
    EXTRA_PATH = { TARGET_DIR..'/disk_c/MSVC20/BIN' }
    EXTRA_VARS = {
        LIB = TARGET_DIR..'/disk_c/MSVC20/LIB',
        INCLUDE = TARGET_DIR..'/disk_c/MSVC20/INCLUDE'
    }
end
