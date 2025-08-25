
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

function msvc2022_32_generate(srcdir, bindir, extra)
    pour.chdir(bindir)
    pour.run('cmake-3.31.4',
            '-A', 'Win32',
            '-G', 'Visual Studio 17 2022',
            unpack(extra or {}),
            srcdir
        )
end

function msvc2022_32(srcdir, bindir, sln, configs, extra)
    pour.chdir(bindir)
    if not pour.file_exists(sln) then
        msvc2022_generate(srcdir, bindir, extra)
    end
    for k, v in ipairs(configs) do
        pour.run('cmake-3.31.4', '--build', '.', '--config', v)
    end
end
