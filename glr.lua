-- A Simple GLR-like (Generalized Backtracking Shift-Reduce / Recursive Descent) Parser Framework
-- It processes a sequence of tokens from the `lexer` library according to a provided grammar.

local GLR = {}

-- A generic packrat/backtracking parser that handles ambiguity by returning all possible parses (GLR style).
-- To prevent infinite recursion on left-recursive grammars without full table generation,
-- we use a limited depth or rewrite the grammar to avoid left recursion.
-- But since this is a simplified GLR engine for demonstration:

local function parse_rule(grammar, rule_name, tokens, start_pos, memo, depth)
    if depth > 500 then return {} end
    local key = rule_name .. ":" .. start_pos
    if memo[key] ~= nil then
        return memo[key]
    end

    local rule_choices = grammar[rule_name]
    if not rule_choices then
        error("Unknown rule: " .. rule_name)
    end

    local results = {}
    -- To prevent direct left recursion infinite loops, we mark as currently parsing:
    memo[key] = {}

    for _, seq in ipairs(rule_choices) do
        local function match_seq(seq_idx, current_pos, current_ast)
            if seq_idx > #seq then
                table.insert(results, { pos = current_pos, ast = { type = rule_name, children = current_ast } })
                return
            end

            local sym = seq[seq_idx]

            if type(sym) == "function" then
                -- custom terminal matcher
                local token = tokens[current_pos]
                if token then
                    local matched, val = sym(token)
                    if matched then
                        local new_ast = {unpack(current_ast)}
                        if val ~= nil then table.insert(new_ast, val) end
                        match_seq(seq_idx + 1, current_pos + 1, new_ast)
                    end
                end
            elseif grammar[sym] then
                -- non-terminal
                local sub_results = parse_rule(grammar, sym, tokens, current_pos, memo, depth + 1)
                for _, res in ipairs(sub_results) do
                    local new_ast = {unpack(current_ast)}
                    table.insert(new_ast, res.ast)
                    match_seq(seq_idx + 1, res.pos, new_ast)
                end
            else
                -- string terminal (exact match)
                local token = tokens[current_pos]
                if token and (token.value == sym or token.type == sym) then
                    local new_ast = {unpack(current_ast)}
                    table.insert(new_ast, token)
                    match_seq(seq_idx + 1, current_pos + 1, new_ast)
                end
            end
        end

        match_seq(1, start_pos, {})
    end

    memo[key] = results
    return results
end

function GLR.parse(grammar, start_rule, tokens)
    local memo = {}
    local results = parse_rule(grammar, start_rule, tokens, 1, memo, 0)

    local full_parses = {}
    for _, res in ipairs(results) do
        if res.pos > #tokens then
            table.insert(full_parses, res.ast)
        end
    end

    return full_parses
end

function GLR.terminal_type(t_type)
    return function(token)
        if token.type == t_type then
            return true, token
        end
        return false
    end
end

function GLR.terminal_value(val)
    return function(token)
        if token.value == val then
            return true, token
        end
        return false
    end
end

return GLR
