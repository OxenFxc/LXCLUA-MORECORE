local lexer = require("lexer")

local code = [[
local function factorial(n)
    local result = 1
    while n > 1 do
        result = result * n
        n = n - 1
    end
    return result
end
print(factorial(5))
]]

local tokens = lexer.lex(code)
local tree = lexer.build_tree(tokens)

local function transform(node)
    if node.type == "Block" and node.opener and node.opener.type == "'while'" then
        node.opener.type = "'if'"
    end

    for _, child in ipairs(node.children or {}) do
        if type(child) == "table" then
            if child.type == "Block" then
                if child.opener and child.opener.type == "'do'" and node.opener and node.opener.type == "'if'" then
                    child.opener.type = "'then'"
                end
                transform(child)
            end
        end
    end
end

transform(tree)

local new_tokens = lexer.flatten_tree(tree)
local transformed_code = lexer.to_code(new_tokens)

print(transformed_code)

-- The spacing might be stripped for tokens without alnum check, e.g. `>` and `1`.
-- Our `to_code` inserts spaces between alphanumeric tokens.
-- "if n>1 then" instead of "if n > 1 then".
-- Let's just remove spaces to assert properly.

local stripped = transformed_code:gsub("%s+", "")
assert(stripped:find("ifn>1then"), "Transformation failed")

print("\nLexer structural utilities tested successfully!")
