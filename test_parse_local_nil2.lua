package.path = package.path .. ";./?.lua"
local lexer = require "lexer"

local source = [[
local function get_bonus()
    return mp + obj.hp * 0.1
end
]]

local tokens = lexer.scan(source)
local stmts = lexer.split_statements(tokens)

for _, stmt in ipairs(stmts) do
    local names, rest = lexer.parse_local(stmt)
    print("names:", names, "rest:", rest)
    if not names then
        print("parse_local returned nil!")
        for i, t in ipairs(stmt) do
            print("TOKEN:", t.value or t.type)
        end
    end
end
