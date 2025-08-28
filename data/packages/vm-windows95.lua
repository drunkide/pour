
SOURCE_URL = 'git@github.com:thirdpartystuff/vm-windows95'
TARGET_DIR = INSTALL_DIR..'/vm-windows95'
INVOKE_LUA = TARGET_DIR..'/config.lua'

if WINDOWS then
    EXTRA_PATH = { TARGET_DIR..'/disk_c/MSDEV/BIN' }
    EXTRA_VARS = {
        LIB = TARGET_DIR..'/disk_c/MSDEV/LIB',
        INCLUDE = TARGET_DIR..'/disk_c/MSDEV/INCLUDE'
    }
end
