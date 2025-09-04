
----------------------------------------------------------------------------------------------------------------------
if HOST_WINDOWS then

CMAKE_VERSION = '3.5.2'
CMAKE_GENERATOR = 'Ninja'

function prepare()
    pour.require('mingw32-4.4.0')
end

function generate()
    CMAKE()
end

function build()
    CMAKE_BUILD()
end

end
----------------------------------------------------------------------------------------------------------------------
