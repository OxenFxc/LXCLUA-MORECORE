local function obf(x)
    if x > 5 then
        x = x * 2
    else
        x = x + 10
    end
    return x
end
local dump = string.dump(obf, false, 43)
local f = io.open("obf2.luac", "wb")
f:write(dump)
f:close()
