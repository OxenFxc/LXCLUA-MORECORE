local plugin = require("plugin")

print("=== Testing Parser ===")
local data = plugin.parse("plugin \"test_pkg\" { version = \"1.2.3\" }")
print("Parsed table: name=" .. tostring(data.name) .. ", version=" .. tostring(data.version))

print("\n=== Testing Dump / Undump ===")
local bdata = plugin.dump(data)
local udata = plugin.undump(bdata)
print("Undumped table: name=" .. tostring(udata.name) .. ", version=" .. tostring(udata.version))

print("\n=== Testing Loader ===")
local res = plugin.load("plugin/test.plugin")
print("Loader returned:")
for k,v in pairs(res) do
  print("  " .. tostring(k) .. " = " .. tostring(v))
end

print("\n=== Testing Shop ===")
local shop_res = plugin.shop_connect()
print("Shop returned: " .. string.sub(shop_res, 1, 100) .. "...")
