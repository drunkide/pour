
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.5.2'
CMAKE_GENERATOR = 'NMake Makefiles'

function prepare()
    pour.fetch('vm-windows95')
end

function generate()
    CMAKE({
        '-DOLD_MSVC=TRUE',
        '-DOLD_MSVC41=TRUE',
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
