
local function run_test(name, func, args, expected)
  print("Testing " .. name .. "...")
  -- 128 is VM_PROTECT flag
  local dumped = string.dump(func, { obfuscate = 128, strip = true })
  local loaded = load(dumped)
  if not loaded then
    error("Failed to load dumped function for " .. name)
  end

  local status, res = pcall(loaded, table.unpack(args or {}))
  if not status then
    error("Error running " .. name .. ": " .. tostring(res))
  end

  if res ~= expected then
    error("Failed " .. name .. ": expected " .. tostring(expected) .. ", got " .. tostring(res))
  end
  print(name .. " passed.")
end

local function tailcall_func(n)
  if n <= 0 then return "done" end
  return tailcall_func(n - 1)
end

-- Wrapper to ensure tailcall_func is the one being dumped/called recursively?
-- Actually, tailcall_func is an upvalue.
-- string.dump dumps the function and its upvalues? No, string.dump doesn't dump upvalues.
-- So we need a self-recursive function that doesn't rely on upvalues (using debug.getinfo?)
-- or just use a global?
-- Or define it inside.
-- Standard Lua tail call test often uses a local function passed as upvalue, but string.dump fails if it has upvalues (unless they are global?).
-- Let's use a global for simplicity in test.
_G.global_tailcall = function(n)
  if n <= 0 then return "done" end
  return _G.global_tailcall(n - 1)
end

run_test("Tail Call", _G.global_tailcall, {100}, "done")

local function setlist_func()
  local t = {}
  for i = 1, 60 do
    t[i] = i
  end
  return t[60]
end

run_test("SETLIST", setlist_func, {}, 60)

-- local function vararg_func(...)
--   local a, b, c = ...
--   return a + b + c
-- end

-- run_test("VARARG", vararg_func, {1, 2, 3}, 6)

print("All tests passed")
