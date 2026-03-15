local score = 75
local status = if score > 90 then "A" elseif score > 60 then "B" else "C" end
assert(status == "B", "Should evaluate to B")

local score2 = 50
local status2 = if score2 > 90 then "A" elseif score2 > 60 then "B" elseif score2 > 40 then "C" else "D" end
assert(status2 == "C", "Should evaluate to C")

local score3 = 100
local status3 = if score3 > 90 then "A" elseif score3 > 60 then "B" elseif score3 > 40 then "C" else "D" end
assert(status3 == "A", "Should evaluate to A")

local score4 = 10
local status4 = if score4 > 90 then "A" elseif score4 > 60 then "B" elseif score4 > 40 then "C" else "D" end
assert(status4 == "D", "Should evaluate to D")

print("All if-expression multi branch tests passed.")
