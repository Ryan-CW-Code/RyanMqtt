from building import *
Import('RTT_ROOT')

# get current directory
cwd = GetCurrentDir()

# The set of source files associated with this SConscript file.
src = Glob('common/*.c')
src += Glob('pahoMqtt/*.c')
src += Glob('mqttclient/*.c')
src += Glob('platform/rtthread/*.c')

path = [cwd + '/common']
path += [cwd + '/pahoMqtt']
path += [cwd + '/mqttclient']
path += [cwd + '/platform/rtthread']

if GetDepend(['PKG_USING_RYANMQTT_EXAMPLE']):
    src += Glob('example/*.c')
    path += [cwd + '/example']

group = DefineGroup('RyanMqtt', src, depend=[
                    'PKG_USING_RYANMQTT'], CPPPATH=path)

Return('group')
