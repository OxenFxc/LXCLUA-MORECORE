print("--- Phase 1: Simple Test ---")
local function simple_test()
  local a = 5
  -- OP_EQI (register == immediate)
  if a == 5 then
     -- This print causes a JIT barrier (OP_GETTABUP), forcing fallback to interpreter
     -- So the EQI is JITed, but the rest (including return) is Interpreted.
     print("Equality check passed")
  end
  return a
end
simple_test()

print("--- Phase 2: Fast Math (Pure JIT) ---")
local function fast_math()
  local a = 100
  -- OP_EQI in JIT currently handles branches by returning to the interpreter (epilogue).
  -- Therefore, execution after this instruction falls back to the interpreter.
  -- You will see [JIT] EQI Helper, but likely not [JIT] Return1.
  if a == 100 then
     -- Empty block
  end
  return a
end
fast_math()

print("--- Phase 3: Caller (Pure JIT Call/Return) ---")
local function target()
  return 1 -- OP_RETURN1 (JIT)
end

local function caller(f)
  f() -- OP_CALL (JIT), calls target
end

-- This test demonstrates OP_CALL and OP_RETURN1 in JIT.
-- [JIT] Call Helper and [JIT] Return1 Helper should appear.
-- Note: The JIT implementation is experimental and may crash on exit due to stack/cleanup issues.
caller(target)

print("--- Done ---")
