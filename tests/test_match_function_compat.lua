-- Test that "match" still works as a function and variable name
local match = string.match
local text = "abc 123 xyz"
local res = match(text, "%d+")
assert(res == "123", "match function failed")

function match_test()
    return "it works"
end
assert(match_test() == "it works")

local data = {type = "player", hp = 5}
match data do
    case {type = "player", hp = x} if x < 10 -> print("Dying!", x)
    case {type = "monster", [1] = id} -> print("attack", id)
    case _ -> print("Unknown")
end
print("Match keyword and match function coexist peacefully!")
