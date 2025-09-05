
SOURCE_URL = 'https://github.com/emscripten-core/emsdk.git'
TARGET_DIR = INSTALL_DIR..'/emsdk-3.1.50'
CHECK_FILE = TARGET_DIR..'/emsdk.bat'
EXTRA_PATH = { TARGET_DIR..'/upstream/emscripten' }

if HOST_WINDOWS then
    POST_FETCH = function()
            pour.require('python3')
            pour.chdir(TARGET_DIR)
            pour.exec('emsdk', 'install', '3.1.50')
            pour.exec('emsdk', 'activate', '3.1.50')
        end
    EXECUTABLE = {
        _default_ = 'emsdk',
        emsdk = TARGET_DIR..'/emsdk.bat',
    }
end
