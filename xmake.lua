csdk_root = "../../../" --csdk根目录,可自行修改
includes(csdk_root.."csdk.lua")
-- description_common()


target("RyanMqtt",function()
    set_kind("static")

    description_csdk()

    --加入代码和头文件
    add_includedirs('./common', {public = true})
    add_includedirs('./pahoMqtt', {public = true})
    add_includedirs('./mqttclient', {public = true})
    add_includedirs('./platform/openLuat', {public = true})

    add_files('./common/*.c', {public = true})
    add_files('./pahoMqtt/*.c', {public = true})
    add_files('./mqttclient/*.c', {public = true})
    add_files('./platform/openLuat/*.c', {public = true})
end)