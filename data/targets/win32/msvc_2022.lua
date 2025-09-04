
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.31.4'
CMAKE_GENERATOR = 'Visual Studio 17 2022'

function prepare()
end

function generate()
    CMAKE({
        '-A', 'Win32',
        })
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
