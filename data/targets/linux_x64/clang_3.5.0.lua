
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.31.4'
CMAKE_GENERATOR = 'Ninja'

function prepare()
    pour.require('clang-3.5.0-linux64')
end

function generate()
    CMAKE({
        '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['clang-3.5.0-linux64']..'/toolchain.cmake',
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
