
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.31.4'
CMAKE_GENERATOR = 'Ninja'

function prepare()
    pour.require('clang-4.0.0-win32')
end

function generate()
    CMAKE({
        '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['clang-4.0.0-win32']..'/toolchain.cmake',
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
