
if HOST_WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/foreign-linux'
    TARGET_DIR = INSTALL_DIR..'/foreign-linux'
    CHECK_FILE = TARGET_DIR..'/_build_clang.cmd'
    POST_FETCH = TARGET_DIR..'/_build_clang.cmd'
    EXTRA_PATH = { TARGET_DIR..'/build/win32-clang_4.0.0-release' }
    ADJUST_ARG = true
    EXECUTABLE = {
        _default_ = 'flinux',
        flinux = TARGET_DIR..'/build/win32-clang_4.0.0-release/flinux.exe',
    }
end
