local GLR = require("glr")
local lexer = require("lexer")

local lua_grammar = {
    program = {
        {"stat", "program_tail"},
        {"stat"}
    },
    program_tail = {
        {"program"}
    },
    stat = {
        {GLR.terminal_type("'local'"), "name", GLR.terminal_type("'='"), "exp"},
        {"name", GLR.terminal_type("'='"), "exp"},
        {"call"},
        {GLR.terminal_type("'if'"), "exp", GLR.terminal_type("'then'"), "program", GLR.terminal_type("'else'"), "program", GLR.terminal_type("'end'")},
        {GLR.terminal_type("'if'"), "exp", GLR.terminal_type("'then'"), "program", GLR.terminal_type("'end'")}
    },
    call = {
        {"name", GLR.terminal_type("'('"), "args", GLR.terminal_type("')'")},
        {"name", GLR.terminal_type("'('"), GLR.terminal_type("')'")}
    },
    args = {
        {"exp", GLR.terminal_type("','"), "args"},
        {"exp"}
    },
    exp = {
        {"term", "binop", "exp"},
        {"term"}
    },
    term = {
        {GLR.terminal_type("<integer>")},
        {GLR.terminal_type("<string>")},
        {"call"},
        {"name"}
    },
    name = {
        {GLR.terminal_type("<name>")},
        {GLR.terminal_type("<identifier>")}
    },
    binop = {
        {GLR.terminal_type("'+'")},
        {GLR.terminal_type("'-'")},
        {GLR.terminal_type("'*'")},
        {GLR.terminal_type("'/'")},
        {GLR.terminal_type("'=='")},
        {GLR.terminal_type("'<'")},
        {GLR.terminal_type("'>'")}
    }
}

local function parse_lua(code)
    local tokens = lexer(code)
    local parses = GLR.parse(lua_grammar, "program", tokens)
    if #parses == 0 then
        -- Dump tokens for debug
        print("Failed to parse. Tokens:")
        for i, t in ipairs(tokens) do print(i, t.type, t.value) end
        error("Syntax error parsing Lua code (subset)")
    end
    -- Pick the first valid parse tree
    return parses[1]
end

local function ast_to_lisp(ast)
    if not ast then return "" end

    if ast.type == "program" then
        local st1 = ast_to_lisp(ast.children[1])
        if #ast.children > 1 then
            local tail = ast_to_lisp(ast.children[2].children[1])
            return st1 .. "\n" .. tail
        end
        return st1
    elseif ast.type == "stat" then
        if ast.children[1].type == "'local'" then
            local name = ast.children[2].children[1].value
            local exp = ast_to_lisp(ast.children[4])
            return "(setq " .. name .. " " .. exp .. ")"
        elseif ast.children[2] and ast.children[2].type == "'='" then
            local name = ast.children[1].children[1].value
            local exp = ast_to_lisp(ast.children[3])
            return "(setq " .. name .. " " .. exp .. ")"
        elseif ast.children[1].type == "call" then
            return ast_to_lisp(ast.children[1])
        elseif ast.children[1].type == "'if'" then
            local cond = ast_to_lisp(ast.children[2])
            local then_branch = "(progn\n  " .. ast_to_lisp(ast.children[4]) .. "\n)"
            local else_branch = "nil"
            if ast.children[5] and ast.children[5].type == "'else'" then
                else_branch = "(progn\n  " .. ast_to_lisp(ast.children[6]) .. "\n)"
            end
            return "(if " .. cond .. " " .. then_branch .. " " .. else_branch .. ")"
        end
    elseif ast.type == "call" then
        local name = ast.children[1].value or ast.children[1].children[1].value
        if ast.children[3] and ast.children[3].type == "args" then
            local args_lisp = ast_to_lisp(ast.children[3])
            return "(" .. name .. " " .. args_lisp .. ")"
        else
            return "(" .. name .. ")"
        end
    elseif ast.type == "args" then
        local e = ast_to_lisp(ast.children[1])
        if #ast.children > 1 then
            return e .. " " .. ast_to_lisp(ast.children[3])
        end
        return e
    elseif ast.type == "exp" then
        if #ast.children == 3 then
            local t1 = ast_to_lisp(ast.children[1])
            local op = ast.children[2].children[1].type
            -- Strip quotes from operator type like "'+'" -> "+"
            op = op:sub(2, -2)
            local t2 = ast_to_lisp(ast.children[3])
            return "(" .. op .. " " .. t1 .. " " .. t2 .. ")"
        else
            return ast_to_lisp(ast.children[1])
        end
    elseif ast.type == "term" then
        if ast.children[1].type == "call" or ast.children[1].type == "name" then
            return ast_to_lisp(ast.children[1])
        else
            local token = ast.children[1]
            if token.type == "<string>" then
                -- Lisp parser string handling
                return '"' .. token.value .. '"'
            end
            return tostring(token.value)
        end
    elseif ast.type == "name" then
        return ast.children[1].value
    end
    return ""
end

local function convert_to_lisp(lua_code)
    local ast = parse_lua(lua_code)
    local lisp_code = "(progn\n" .. ast_to_lisp(ast) .. "\n)"
    return lisp_code
end

-- Function to generate the final obfuscated file content
local function generate_obfuscated_file(lua_code)
    local lisp_str = convert_to_lisp(lua_code)

    local out = [=[
local lisp_runner = require("lisp_glr")
local lisp_code = ]=] .. string.format("%q", lisp_str) .. [=[

-- Execute the transpiled lisp code
lisp_runner.run(lisp_code)
]=]
    return out
end

return {
    convert = convert_to_lisp,
    obfuscate = generate_obfuscated_file,
    grammar = lua_grammar
}
