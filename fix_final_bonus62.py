import os
import re

with open("lexer_cff_vmp_v12.lua", "r") as f:
    content = f.read()

# Fix 1: Change to read test_bonus_small.lua instead of hardcoded code
content = re.sub(
    r"local code = \[\[.*?\]\]",
    """local f = io.open(arg[1] or "test_bonus_small.lua", "r")
local code = f:read("*a")
f:close()""",
    content,
    flags=re.DOTALL
)

# Fix 2: Pre-populate global locals so they aren't mangled into `_G["..."]`
patch_collect = """
local function collect_locals(node, map)
    if not node.elements then return end
    for i=1, #node.elements do
        local el = node.elements[i]
        if el.type == "'local'" then
            local next_el = node.elements[i+1]
            if next_el and next_el.type == "<name>" then
                map[next_el.value] = next_el.value
            elseif next_el and next_el.type == "function" then
                local fnode = next_el
                if fnode.elements[2] and fnode.elements[2].type == "<name>" then
                    map[fnode.elements[2].value] = fnode.elements[2].value
                end
            end
        end
        if el.type ~= "function" then
            collect_locals(el, map)
        end
    end
end
"""

content = content.replace("local function flatten_v11(func_node)", patch_collect + "\nlocal function flatten_v11(func_node, outer_locals)")

content = content.replace("""    local var_map = {}
    local next_v_idx = 1""", """    local var_map = {}
    for k, v in pairs(outer_locals or {}) do var_map[k] = v end
    local next_v_idx = 1""")

content = content.replace("local function walk(node)", "local function walk(node, outer_locals)")
content = content.replace("flatten_v11(node)", "flatten_v11(node, outer_locals)")
content = content.replace("flatten_v11(node.elements[i+1])", "flatten_v11(node.elements[i+1], outer_locals)")
content = content.replace("walk(el)", "walk(el, outer_locals)")

content = content.replace("""local function obfuscate(code)
    local tokens = lexer(code)
    local tree = lexer.build_tree(tokens)
    return walk(tree)
end""", """local function obfuscate(code)
    local tokens = lexer(code)
    local tree = lexer.build_tree(tokens)
    local outer_locals = {}
    collect_locals(tree, outer_locals)
    return walk(tree, outer_locals)
end""")


# Fix 3: Handle fs_reconstruct safely
fix_reconstruct = """        if t.type == "<name>" and var_map[v] then
            if type(var_map[v]) == "number" then
                fs_reconstruct = fs_reconstruct .. "__M[" .. encrypt_index(var_map[v]) .. "] "
            else
                fs_reconstruct = fs_reconstruct .. var_map[v] .. " "
            end
        else fs_reconstruct = fs_reconstruct .. v .. " " end"""
content = re.sub(r"if t\.type == \"<name>\" and var_map\[v\] then.*?else fs_reconstruct = fs_reconstruct \.\. v \.\. \" \" end", fix_reconstruct, content, flags=re.DOTALL)


with open("lexer_cff_vmp_v12.lua", "w") as f:
    f.write(content)
