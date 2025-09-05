
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.5.2'
CMAKE_GENERATOR = 'Ninja'

function prepare()
    pour.require('emsdk-3.1.50')
    pour.require('python3')
end

function generate()
    CMAKE({
        '-DCMAKE_TOOLCHAIN_FILE='..
            PACKAGE_DIR['emsdk-3.1.50']..'/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
