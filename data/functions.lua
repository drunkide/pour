
CMAKE_CONFIGURATIONS = { 'Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel' }

function table_append(dst, src)
    if type(src) ~= 'table' then
        table.insert(dst, tostring(src))
    else
        for _, v in ipairs(src) do
            table.insert(dst, tostring(v))
        end
    end
end

function CMAKE(params)
    local e = {}
    table_append(e, '--no-warn-unused-cli')
    table_append(e, '-DCMAKE_MODULE_PATH='..table.concat(CMAKE_MODULE_PATH, ';'))
    if CMAKE_GENERATOR ~= '' then
        table_append(e, { '-G', CMAKE_GENERATOR })
        if HOST_WINDOWS then
            if CMAKE_GENERATOR == 'Ninja' then
                table_append(e, '-DCMAKE_MAKE_PROGRAM='..PACKAGE_DIR['ninja']..'/ninja.exe')
            end
        end
    end
    if CMAKE_PARAMS then
        table_append(e, CMAKE_PARAMS)
    end
    if params then
        table_append(e, params)
    end
    if VERBOSE then
        table_append(e, '-DCMAKE_VERBOSE_MAKEFILE=ON')
    end
    if not CMAKE_IS_MULTICONFIG then
        table_append(e, '-DCMAKE_BUILD_TYPE='..CMAKE_CONFIGURATION)
    end
    table_append(e, SOURCE_DIR)
    cmake_generate(CMAKE_VERSION, table.unpack(e))
end

function CMAKE_BUILD(params)
    local e = {}
    if CMAKE_FORCE_REBUILD then
        table_append(e, '--clean-first')
    end
    if VERBOSE and CMAKE_VERSION ~= '3.5.2' then
        table_append(e, '--verbose')
    end
    if CMAKE_IS_MULTICONFIG then
        if CMAKE_CONFIGURATION then
            table_append(e, { '--config', CMAKE_CONFIGURATION })
        else
            for _, config in ipairs(CMAKE_CONFIGURATIONS) do
                CMAKE_CONFIGURATION = config
                CMAKE_BUILD(params)
            end
            return
        end
    end
    if CMAKE_BUILD_PARAMS then
        table_append(e, CMAKE_BUILD_PARAMS)
    end
    if params then
        table_append(e, params)
    end
    pour.run('cmake-'..CMAKE_VERSION, '--build', '.', table.unpack(e))
end
