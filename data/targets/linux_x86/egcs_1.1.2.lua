
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.31.4'
CMAKE_GENERATOR = 'MinGW Makefiles'

function prepare()
    pour.require('egcs-1.1.2')
end

function generate()
    CMAKE({
        '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['egcs-1.1.2']..'/tools/cmake_toolchain/linux-egcs.cmake',
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
