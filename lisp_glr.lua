local GLR = require("glr")
local lexer = require("lexer")

-- Lisp Grammar Definition
local lisp_grammar = {
    program = {
        {"expr", "program_tail"},
        {"expr"}
    },
    program_tail = {
        {"program"}
    },
    expr = {
        {"atom"},
        {"list"}
    },
    list = {
        {GLR.terminal_type("'('"), "expr_list", GLR.terminal_type("')'")},
        {GLR.terminal_type("'('"), GLR.terminal_type("')'")}
    },
    expr_list = {
        {"expr", "expr_list"},
        {"expr"}
    },
    atom = {
        {GLR.terminal_type("<identifier>")},
        {GLR.terminal_type("<name>")},
        {GLR.terminal_type("<integer>")},
        {GLR.terminal_type("<string>")},
        {GLR.terminal_type("'+'")},
        {GLR.terminal_type("'-'")},
        {GLR.terminal_type("'*'")},
        {GLR.terminal_type("'/'")},
        {GLR.terminal_type("'=='")},
        {GLR.terminal_type("'<'")},
        {GLR.terminal_type("'>'")},
        {GLR.terminal_type("'='")},
        {GLR.terminal_type("'if'")},
        {GLR.terminal_type("'true'")},
        {GLR.terminal_type("'false'")}
    }
}

local function parse_lisp(code)
    local tokens = lexer(code)
    local parses = GLR.parse(lisp_grammar, "program", tokens)
    if #parses == 0 then
        print("Lisp syntax error. Tokens:")
        for i, t in ipairs(tokens) do print(i, t.type, t.value) end
        error("Syntax error parsing Lisp code")
    end
    return parses[1]
end

-- Evaluator
local function eval_ast(ast, env)
    if not ast then return nil end
    if ast.type == "program" then
        local last_val = nil
        for _, child in ipairs(ast.children) do
            if child.type == "expr" then
                last_val = eval_ast(child, env)
            elseif child.type == "program_tail" then
                last_val = eval_ast(child.children[1], env)
            end
        end
        return last_val
    elseif ast.type == "expr" then
        return eval_ast(ast.children[1], env)
    elseif ast.type == "atom" then
        local token = ast.children[1]
        local val = token.value
        local t_type = token.type
        if t_type == "<integer>" then return tonumber(val)
        elseif t_type == "<string>" then return val
        else
            -- Check for exact token matches (like '+') vs value (like 'a')
            local sym_val = val or t_type:sub(2, -2)
            if sym_val == "true" then return true end
            if sym_val == "false" then return false end
            if env[sym_val] ~= nil then return env[sym_val] end
            return sym_val
        end
    elseif ast.type == "list" then
        if #ast.children == 2 then return nil end -- empty list ()
        local expr_list = ast.children[2]
        local args = {}

        -- To avoid evaluating both branches of if, we need to handle special forms first.
        -- We extract the operator name first:
        local op_node = expr_list.children[1]
        local op_val = nil
        if op_node.type == "expr" and op_node.children[1].type == "atom" then
            local token = op_node.children[1].children[1]
            op_val = token.value or token.type:sub(2, -2)
        end

        -- Special Forms
        if op_val == "setq" or op_val == "set" or op_val == "=" then
            local var_name_ast = expr_list.children[2].children[1].children[1].children[1]
            local var_name = var_name_ast.value or var_name_ast.type:sub(2, -2)
            local val_ast = expr_list.children[2].children[2].children[1]
            local val = eval_ast(val_ast, env)
            env[var_name] = val
            return val
        elseif op_val == "if" then
            local cond_ast = expr_list.children[2].children[1]
            local then_ast = expr_list.children[2].children[2].children[1]
            local else_ast = expr_list.children[2].children[2].children[2] and expr_list.children[2].children[2].children[2].children[1] or nil

            local cond = eval_ast(cond_ast, env)
            if cond then
                return eval_ast(then_ast, env)
            else
                if else_ast then
                    return eval_ast(else_ast, env)
                end
                return nil
            end
        elseif op_val == "progn" or op_val == "do" then
            local last_val = nil
            local cur = expr_list.children[2]
            while cur do
                last_val = eval_ast(cur.children[1], env)
                if #cur.children > 1 then
                    cur = cur.children[2]
                else
                    cur = nil
                end
            end
            return last_val
        end

        -- Regular Function Call
        local function collect_args(el)
            local res = eval_ast(el.children[1], env)
            table.insert(args, res)
            if #el.children > 1 then
                collect_args(el.children[2])
            end
        end
        collect_args(expr_list)

        local op = args[1]
        if type(op) == "function" then
            return op(unpack(args, 2))
        end

        -- Otherwise treat as function call on environment lookup
        local func = env[op]
        if type(func) == "function" then
            return func(unpack(args, 2))
        end

        if op == nil and #args > 0 then
            return nil
        end

        error("Unknown function or operator: " .. tostring(op))
    end
end

local function run_lisp(code, custom_env)
    local ast = parse_lisp(code)
    local env = custom_env or {
        ["+"] = function(a, b) return a + b end,
        ["-"] = function(a, b) return a - b end,
        ["*"] = function(a, b) return a * b end,
        ["/"] = function(a, b) return a / b end,
        ["=="] = function(a, b) return a == b end,
        ["<"] = function(a, b) return a < b end,
        [">"] = function(a, b) return a > b end,
        ["print"] = print
    }
    return eval_ast(ast, env)
end

return {
    parse = parse_lisp,
    run = run_lisp,
    grammar = lisp_grammar
}
