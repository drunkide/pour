
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.31.4'
CMAKE_GENERATOR = 'MinGW Makefiles'

function prepare()
    pour.require('watcom-10.0a')
end

function generate()
    CMAKE({
        '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['watcom-10.0a']..'/toolchain-win32.cmake',
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
