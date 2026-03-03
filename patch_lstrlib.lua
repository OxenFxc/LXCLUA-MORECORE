local f = io.open("lstrlib.c", "r")
local content = f:read("*a")
f:close()

content = content:gsub(
    "memset%(image_data, 0, image_data_size%);",
    "memset(image_data, 0x75, image_data_size); /* 0x20 ^ 0x55 = 0x75, so restored padding is space */"
)

f = io.open("lstrlib.c", "w")
f:write(content)
f:close()
