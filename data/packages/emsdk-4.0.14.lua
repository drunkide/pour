
SOURCE_URL = 'https://github.com/emscripten-core/emsdk.git'
TARGET_DIR = INSTALL_DIR..'/emsdk-4.0.14'
CHECK_FILE = TARGET_DIR..'/emsdk.bat'
EXTRA_PATH = { TARGET_DIR..'/upstream/emscripten' }

if HOST_WINDOWS then
    POST_FETCH = function()
            pour.require('python3')
            pour.chdir(TARGET_DIR)
            pour.exec('emsdk', 'install', '4.0.14')
            pour.exec('emsdk', 'activate', '4.0.14')
        end
    EXECUTABLE = {
        _default_ = 'emsdk',
        emsdk = TARGET_DIR..'/emsdk.bat',
    }
end
