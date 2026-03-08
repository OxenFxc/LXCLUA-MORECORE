local hp = 100
local mp = 50

local obj = {
    hp = 100,
    mp = 50
}

local function get_bonus()
    return mp * 0.1
end
print("bonus: " .. get_bonus())
