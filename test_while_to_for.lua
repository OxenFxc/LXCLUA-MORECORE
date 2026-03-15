local lexer = require("lexer")

local code = [[
local i = 1
while i <= 5 do
    print(i)
    i = i + 1
end
]]

print("--- Original Code ---")
print(code)

local tokens = lexer(code)

-- Find the while loop
local while_idx
for i, t in ipairs(tokens) do
    if t.type == "'while'" then
        while_idx = i
        break
    end
end

if while_idx then
    local do_idx
    for i = while_idx, #tokens do
        if tokens[i].type == "'do'" then
            do_idx = i
            break
        end
    end

    local end_idx = lexer.find_match(tokens, while_idx)

    if do_idx and end_idx then
        -- Extract the condition
        local condition_tokens = {}
        for i = while_idx + 1, do_idx - 1 do
            table.insert(condition_tokens, tokens[i])
        end
        local condition_str = lexer.reconstruct(condition_tokens)

        -- Extract the body
        local body_tokens = {}
        for i = do_idx + 1, end_idx - 1 do
            table.insert(body_tokens, tokens[i])
        end
        local body_str = lexer.reconstruct(body_tokens)

        -- Reconstruct as a for loop using a break condition
        -- for _ = 1, math.huge do
        --     if not (condition) then break end
        --     body
        -- end

        local new_loop_code = string.format([[
for _ = 1, math.huge do
    if not (%s) then break end
%s
end
]], condition_str, body_str)

        print("--- Transformed to For Loop ---")
        print(new_loop_code)

        -- Execute the transformed code to verify
        local full_new_code = "local i = 1\n" .. new_loop_code
        print("--- Execution Output ---")
        load(full_new_code)()
    end
end
