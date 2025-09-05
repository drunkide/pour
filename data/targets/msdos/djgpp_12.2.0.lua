
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.5.2'
CMAKE_GENERATOR = 'Ninja'

function prepare()
    pour.require('djgpp-12.2.0')
    table_append(CMAKE_MODULE_PATH, PACKAGE_DIR['djgpp-12.2.0']..'/cmake/Modules')
end

function generate()
    CMAKE({
        '-DCMAKE_TOOLCHAIN_FILE='..PACKAGE_DIR['djgpp-12.2.0']..'/cmake/toolchain.cmake',
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
