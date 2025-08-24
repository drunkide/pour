
if WINDOWS then
    SOURCE_URL = 'https://github.com/thirdpartystuff/foreign-linux'
    TARGET_DIR = TOOLS_DIR..'/foreign-linux'
    CHECK_FILE = TARGET_DIR..'/_build_clang.cmd'
    POST_FETCH = TARGET_DIR..'/_build_clang.cmd'
    EXTRA_PATH = { TARGET_DIR..'/build/clang' }
    EXECUTABLE = {
        _default_ = 'flinux',
        flinux = TARGET_DIR..'/build/clang/flinux.exe',
    }
end
