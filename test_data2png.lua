local test_str = 'print("hello from data2png")'
local str = require("string")
local fname = "test_data_out.png"
local png_data = str.data2png(test_str)
local f = io.open(fname, "wb")
f:write(png_data)
f:close()
local loaded, err = loadfile(fname)
if loaded then
    print("loaded successfully!")
    loaded()
else
    print("load failed:", err)
end
