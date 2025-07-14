add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
target("RyanMqtt",function()
    set_kind("binary")

    add_syslinks("pthread")
    set_toolchains("gcc")  -- 确保使用 GCC
    -- set_toolchains("clang-20")  -- 确保使用 GCC
    set_languages("gnu99") -- 关键！启用 GNU 扩展
    set_warnings("everything") -- 启用全部警告 -Wall -Wextra -Weffc++ / -Weverything

    set_optimize("smallest") -- -Os
    -- set_optimize("faster") -- -O2
    -- set_optimize("fastest") -- -O3

    add_defines("PKG_USING_RYANMQTT_IS_ENABLE_ASSERT") -- 开启assert

    add_cxflags(
                "-pedantic",  
                "-Wall",
                "-Wextra",
                "-Wno-unused-parameter",
                -- clang的
                -- "-Wno-gnu-zero-variadic-macro-arguments",
                -- "-Wno-c23-extensions",
                {force=true})
    
    --加入代码和头文件
    add_includedirs('./common', {public = true})
    add_includedirs('./coreMqtt', {public = true})
    add_includedirs('./mqttclient/include', {public = true})
    add_includedirs('./platform/linux', {public = true})
    add_includedirs('./platform/linux/valloc', {public = true})

    add_files('./test/*.c', {public = true})
    add_files('./common/*.c', {public = true})
    add_files('./coreMqtt/*.c', {public = true})
    add_files('./mqttclient/*.c', {public = true})
    add_files('./platform/linux/*.c', {public = true})
    add_files('./platform/linux/valloc/*.c', {public = true})

          add_ldflags("-Wl,-Map=$(buildir)/RyanMqtt.map") 
end)