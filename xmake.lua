add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
target("RyanMqtt",function()
    set_kind("binary")

    add_syslinks("pthread")
    set_toolchains("gcc")  -- 确保使用 GCC
    set_languages("gnu99") -- 关键！启用 GNU 扩展

    -- set_optimize("smallest") -- -Os
    -- set_optimize("faster") -- -O2
    set_optimize("fastest") -- -O3

    add_cxflags(
                -- "-fanalyzer",
                "-pedantic",  
                "-Wall",
                "-Wextra",
                "-Wno-unused-parameter",
                "-Wincompatible-pointer-types",
                "-Werror=incompatible-pointer-types",
                "-Wno-error=pointer-to-int-cast",
                {force=true})

    --加入代码和头文件
    add_includedirs('./common', {public = true})
    add_includedirs('./coreMqtt', {public = true})
    add_includedirs('./mqttclient', {public = true})
    add_includedirs('./platform/linux', {public = true})
    add_includedirs('./platform/linux/valloc', {public = true})

    add_files('./test/*.c', {public = true})
    add_files('./common/*.c', {public = true})
    add_files('./coreMqtt/*.c', {public = true})
    add_files('./mqttclient/*.c', {public = true})
    add_files('./platform/linux/*.c', {public = true})
    add_files('./platform/linux/valloc/*.c', {public = true})
end)