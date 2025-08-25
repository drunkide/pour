
----------------------------------------------------------------------------------------------------------------------

function mingw32_810_generate(srcdir, bindir, buildtype)
    pour.require("mingw32-8.1.0")
    pour.require("ninja")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function mingw32_810(srcdir, bindir, buildtype, exe)
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        mingw32_810_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end

----------------------------------------------------------------------------------------------------------------------

function mingw64_810_generate(srcdir, bindir, buildtype)
    pour.require("mingw64-8.1.0")
    pour.require("ninja")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function mingw64_810(srcdir, bindir, buildtype, exe)
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        mingw64_810_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end

----------------------------------------------------------------------------------------------------------------------

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

----------------------------------------------------------------------------------------------------------------------

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

----------------------------------------------------------------------------------------------------------------------

function egcs_112_generate(srcdir, bindir, buildtype)
    pour.require("egcs-1.1.2")
    pour.require("make")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'MinGW Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['egcs-1.1.2']..'/tools/cmake_toolchain/linux-egcs.cmake',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            srcdir
        )
end

function egcs_112(srcdir, bindir, buildtype, exe)
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        egcs_112_generate(srcdir, bindir, buildtype)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end
