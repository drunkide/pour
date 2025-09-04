
function table_append(dst, src)
    if type(src) ~= 'table' then
        table.insert(dst, tostring(src))
    else
        for _, v in ipairs(src) do
            table.insert(dst, tostring(v))
        end
    end
end

function CMAKE(params)
    local e = {}
    if CMAKE_GENERATOR ~= '' then
        table_append(e, { '-G', CMAKE_GENERATOR })
    end
    if CMAKE_PARAMS then
        table_append(e, CMAKE_PARAMS)
    end
    if params then
        table_append(e, params)
    end
    if VERBOSE then
        table_append(e, '-DCMAKE_VERBOSE_MAKEFILE=ON')
    end
    if not CMAKE_IS_MULTICONFIG then
        table_append(e, '-DCMAKE_BUILD_TYPE='..CMAKE_CONFIGURATION)
    end
    table_append(e, SOURCE_DIR)
    pour.run('cmake-'..CMAKE_VERSION, table.unpack(e))
end

function CMAKE_BUILD(params)
    local e = {}
    if CMAKE_FORCE_REBUILD then
        table_append(e, '--clean-first')
    end
    if VERBOSE and CMAKE_VERSION ~= '3.5.2' then
        table_append(e, '--verbose')
    end
    if CMAKE_IS_MULTICONFIG then
        table_append(e, { '--config', CMAKE_CONFIGURATION })
    end
    if CMAKE_BUILD_PARAMS then
        table_append(e, CMAKE_BUILD_PARAMS)
    end
    if params then
        table_append(e, params)
    end
    pour.run('cmake-'..CMAKE_VERSION, '--build', '.', table.unpack(e))
end

----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

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
if HOST_WINDOWS then

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
if HOST_WINDOWS then

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
if HOST_WINDOWS then

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
if HOST_WINDOWS then

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
if HOST_WINDOWS then

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
if HOST_WINDOWS then

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
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

function borland_452_win32_generate(srcdir, bindir, buildtype, extra)
    local e = { table.unpack(extra or {}) }
    e[#e + 1] = srcdir
    pour.require("make")
    pour.require("borland-4.5.2")
    pour.chdir(bindir)
    pour.run('cmake-3.5.2',
            '-G', 'MinGW Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['borland-4.5.2']..'/toolchain-win32.cmake',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            table.unpack(e)
        )
end

function borland_452_win32(srcdir, bindir, buildtype, exe, extra)
    pour.require("make")
    pour.require("borland-4.5.2")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        borland_452_win32_generate(srcdir, bindir, buildtype, extra)
    end
    pour.run('cmake-3.5.2', '--build', '.')
end

end
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

function watcom_10_win32_generate(srcdir, bindir, buildtype, extra)
    local e = { table.unpack(extra or {}) }
    e[#e + 1] = srcdir
    pour.require("make")
    pour.require("watcom-10.0a")
    pour.chdir(bindir)
    pour.run('cmake-3.31.4',
            '-G', 'MinGW Makefiles',
            '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['watcom-10.0a']..'/toolchain-win32.cmake',
            '-DCMAKE_BUILD_TYPE='..buildtype,
            table.unpack(e)
        )
end

function watcom_10_win32(srcdir, bindir, buildtype, exe, extra)
    pour.require("make")
    pour.require("borland-4.5.2")
    pour.chdir(bindir)
    if not pour.file_exists(exe) then
        watcom_10_win32_generate(srcdir, bindir, buildtype, extra)
    end
    pour.run('cmake-3.31.4', '--build', '.')
end

end
