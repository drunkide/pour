
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.5.2'
CMAKE_GENERATOR = 'MinGW Makefiles'

function prepare()
    pour.require('borland-4.5.2')
end

function generate()
    CMAKE({
        '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['borland-4.5.2']..'/toolchain-win32.cmake',
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
