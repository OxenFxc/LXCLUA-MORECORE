local code = [[
ADD_OPCODE "MY_OP" "0x99"
SET_ENV "HELLO" "WORLD"
RUN_LUA 'print("Plugin running dynamically!")'
]]

plugin.load(code)
print("HELLO is:", HELLO)
