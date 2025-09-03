
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function mingw32_440_generate(srcdir, bindir, buildtype)
    pour.require("ninja")
    pour.require("mingw32-4.4.0")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function mingw32_440(srcdir, bindir, buildtype, exe)
    pour.require("ninja")
    pour.require("mingw32-4.4.0")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        mingw32_440_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function mingw32_810_generate(srcdir, bindir, buildtype)
    pour.require("ninja")
    pour.require("mingw32-8.1.0")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function mingw32_810(srcdir, bindir, buildtype, exe)
    pour.require("ninja")
    pour.require("mingw32-8.1.0")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        mingw32_810_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function mingw64_810_generate(srcdir, bindir, buildtype)
    pour.require("ninja")
    pour.require("mingw64-8.1.0")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function mingw64_810(srcdir, bindir, buildtype, exe)
    pour.require("ninja")
    pour.require("mingw64-8.1.0")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        mingw64_810_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function clang_350_linux64_generate(srcdir, bindir, buildtype, extra)
    local e = { table.unpack(extra or {}) }
    e[#e + 1] = srcdir
    pour.require("ninja")
    pour.require("clang-3.5.0-linux64")
    pour.chdir(bindir)
    pour.run('cmake-3.31.4',
            '-G', 'Ninja',
            '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['clang-3.5.0-linux64']..'/toolchain.cmake',
            '-DCMAKE_MAKE_PROGRAM='..PACKAGE_DIR['ninja']..'/ninja.exe',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            table.unpack(e)
        )
end

function clang_350_linux64(srcdir, bindir, buildtype, exe, extra)
    pour.require("ninja")
    pour.require("clang-3.5.0-linux64")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        clang_350_linux64_generate(srcdir, bindir, buildtype, extra)
    end
    pour.run('cmake-3.31.4', '--build', '.')
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function clang_400_win32_generate(srcdir, bindir, buildtype, extra)
    local e = { table.unpack(extra or {}) }
    e[#e + 1] = srcdir
    pour.require("ninja")
    pour.require("clang-4.0.0-win32")
    pour.chdir(bindir)
    pour.run('cmake-3.31.4',
            '-G', 'Ninja',
            '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['clang-4.0.0-win32']..'/toolchain.cmake',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            table.unpack(e)
        )
end

function clang_400_win32(srcdir, bindir, buildtype, exe, extra)
    pour.require("ninja")
    pour.require("clang-4.0.0-win32")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        clang_400_win32_generate(srcdir, bindir, buildtype, extra)
    end
    pour.run('cmake-3.31.4', '--build', '.')
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function msvc20_generate(srcdir, bindir, buildtype)
    pour.fetch("vm-windows-nt31")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'NMake Makefiles',
            '-DOLD_MSVC=TRUE',
            '-DOLD_MSVC20=TRUE',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function msvc20(srcdir, bindir, buildtype, exe)
    pour.fetch("vm-windows-nt31")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        msvc20_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function msvc41_generate(srcdir, bindir, buildtype)
    pour.fetch("vm-windows95")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'NMake Makefiles',
            '-DOLD_MSVC=TRUE',
            '-DOLD_MSVC41=TRUE',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function msvc41(srcdir, bindir, buildtype, exe)
    pour.fetch("vm-windows95")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        msvc41_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function msvc2022_32_generate(srcdir, bindir, extra)
    local e = { table.unpack(extra or {}) }
    e[#e + 1] = srcdir
    pour.chdir(bindir)
    pour.run('cmake-3.31.4',
            '-A', 'Win32',
            '-G', 'Visual Studio 17 2022',
            table.unpack(e)
        )
end

function msvc2022_32(srcdir, bindir, sln, configs, extra)
    pour.chdir(bindir)
    if not pour.file_exists(sln) then
        msvc2022_32_generate(srcdir, bindir, extra)
    end
    for k, v in ipairs(configs) do
        pour.run('cmake-3.31.4', '--build', '.', '--config', v)
    end
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function msvc2022_64_generate(srcdir, bindir, extra)
    local e = { table.unpack(extra or {}) }
    e[#e + 1] = srcdir
    pour.chdir(bindir)
    pour.run('cmake-3.31.4',
            '-A', 'x64',
            '-G', 'Visual Studio 17 2022',
            table.unpack(e)
        )
end

function msvc2022_64(srcdir, bindir, sln, configs, extra)
    pour.chdir(bindir)
    if not pour.file_exists(sln) then
        msvc2022_64_generate(srcdir, bindir, extra)
    end
    for k, v in ipairs(configs) do
        pour.run('cmake-3.31.4', '--build', '.', '--config', v)
    end
end

end
----------------------------------------------------------------------------------------------------------------------
if WINDOWS then

function egcs_112_generate(srcdir, bindir, buildtype)
    pour.require("make")
    pour.require("egcs-1.1.2")
    pour.chdir(bindir)
    pour.run('cmake-3.31.4',
            '-G', 'MinGW Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['egcs-1.1.2']..'/tools/cmake_toolchain/linux-egcs.cmake',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function egcs_112(srcdir, bindir, buildtype, exe)
    pour.require("make")
    pour.require("egcs-1.1.2")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        egcs_112_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.31.4', '--build', '.')
end

end
