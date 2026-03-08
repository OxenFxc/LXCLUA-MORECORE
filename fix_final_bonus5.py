# Ah, OK, so `lexer.parse_local(stmt)` returns `nil` for `local function`.
# That means in `lexer_cff_vmp_v12.lua` it executes the `else` block:
# ```lua
#        else
#            local vmp_stmt = try_compile_to_vmp(stmt, var_map, next_v_idx, DYN_VMP, DYN_M)
#            local mapped_stmt = {}
#            transform_tokens(vmp_stmt or stmt, mapped_stmt)
#            if mapped_stmt then
#                table.insert(statements_out, mapped_stmt)
#            end
#        end
# ```
# Let's check `try_compile_to_vmp` inside `lexer_cff_vmp_v12.lua`.
# For `local function get_bonus()`, what does it do?
# It doesn't match any `A = ...` patterns.
# So it falls through to:
# ```lua
#        if not matched then
#            table.insert(final_tokens, stmt_tokens[i])
#            i = i + 1
#        end
# ```
# Since it doesn't compile anything, `compiled_anything` is `false`, and it returns `nil`.
#
# Then `transform_tokens` is called with `stmt`.
# In `transform_tokens`:
# ```lua
#            elseif r.type == "<name>" then
#                if next_val == "=" and prev_val == "{" or next_val == "=" and prev_val == "," then
#                    ...
#                elseif prev_val == "." then
#                    ...
#                elseif var_map[r.value] then
#                    ...
#                elseif is_keyword(r.value) then table.insert(out_stmt, r)
#                else
#                    if prev_val == ":" then table.insert(out_stmt, r)
#                    elseif prev_val == "for" or prev_val == "function" then
#                        print("OVERWRITING VAR MAP FOR:", r.value, "PREV:", prev_val); var_map[r.value] = r.value
#                        table.insert(out_stmt, r)
#                    elseif prev_val == "local" then
#                        -- DO NOT sabotage local variables! They are already mapped.
#                        table.insert(out_stmt, r)
#                    else
#                        if var_map[r.value] == r.value then table.insert(out_stmt, r)
#                        else
#                            table.insert(out_stmt, {token=0, type="<raw>", value="_G", line=r.line})
#                            table.insert(out_stmt, {token=0, type="'['", value="[", line=r.line})
#                            local g_toks = split_string_anti_hook('"'..r.value..'"', r.line)
#                            for _, st in ipairs(g_toks) do table.insert(out_stmt, st) end
#                            table.insert(out_stmt, {token=0, type="']'", value="]", line=r.line})
#                        end
#                    end
#                end
#            else table.insert(out_stmt, r) end
# ```
# `local function get_bonus() return mp + obj.hp * 0.1 end`
# Token 1: `local`. Type: `"<keyword>"`. So `table.insert(out_stmt, r)`. `prev_val`="local".
# Token 2: `function`. Type: `"<keyword>"`. `table.insert(out_stmt, r)`. `prev_val`="function".
# Token 3: `get_bonus`. Type: `"<name>"`.
#     Is it in `var_map`? Wait! DID `extract_locals_deep` PUT `get_bonus` IN `var_map`?!
# Let's check `extract_locals_deep`.
# It processes `local function NAME`. Wait, no it doesn't!
# ```lua
#            elseif r.value == "function" and prev_val == "local" then
#                local next_tok = tokens[i+1]
#                if next_tok and next_tok.type == "<name>" then
#                    if not var_map[next_tok.value] then
#                        var_map[next_tok.value] = next_v_idx; next_v_idx = next_v_idx + 1
#                    end
#                end
# ```
# Ah! It DID put `get_bonus` into `var_map` as a NUMBER!
# So for token 3 (`get_bonus`), `var_map["get_bonus"]` is a number!
# So it hits `elseif var_map[r.value] then`:
# ```lua
#                elseif var_map[r.value] then
#                    if r.value == "bonus" then print("PROCESSING BONUS, VAR_MAP VAL:", var_map[r.value], "TYPE:", type(var_map[r.value])) end
#                    if prev_val == ":" then table.insert(out_stmt, r)
#                    elseif type(var_map[r.value]) == "string" then table.insert(out_stmt, r)
#                    else table.insert(out_stmt, {token=0, type="<raw>", value="__M[" .. encrypt_index(var_map[r.value]) .. "]", line=r.line}) end
# ```
# So it transforms `get_bonus` into `__M[123]`!
# BUT WAIT.
# Look at the error:
# "local function __M[123]() ... end" -> SYNTAX ERROR in Lua!
# You cannot do `local function __M[123]()` !!
# Wait, NO. If it is `local function __M[123]()`, that's illegal!
# BUT the grep log said:
# ```
# local function get_bonus ( )
# ```
# Why did it output `get_bonus` and NOT `__M[123]` ??
#
# Because maybe `get_bonus` was NOT in `var_map`?!
# If `get_bonus` was NOT in `var_map`, then it falls to the `else` block:
# ```lua
#                    elseif prev_val == "for" or prev_val == "function" then
#                        print("OVERWRITING VAR MAP FOR:", r.value, "PREV:", prev_val); var_map[r.value] = r.value
#                        table.insert(out_stmt, r)
# ```
# And it outputs `get_bonus`! AND overwrites `var_map["get_bonus"] = "get_bonus"` (a string)!
#
# Let's trace Token 4: `(`. Type `"<operator>"`.
# `table.insert(out_stmt, r)`. `prev_val`="(".
# Token 5: `)`. Type `"<operator>"`.
# `table.insert(out_stmt, r)`. `prev_val`=")`.
# Token 6: `\n`. Wait, `safe_split_statements` strips newlines if we didn't add them. But we DO add them now!
# Token 7: `return`. Wait!
# Does it output `return` ?
# If it outputs `return`, why did grep output ONLY `local function get_bonus ( ) ` ?
# Maybe grep ONLY found the first line because of `\n`?!
# YES!!! `grep` searches line by line!
# The output file HAS `return` on the next line!
# LET'S CHECK THE GENERATED OBFUSCATED FILE!
