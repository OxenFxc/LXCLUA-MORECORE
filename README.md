# LXCLUA-NCore

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C23-blue.svg)]()
[![Platform](https://img.shields.io/badge/Platform-Cross--Platform-green.svg)]()

A high-performance embedded scripting engine based on **Lua 5.5 (Custom)** with enhanced security features, extended libraries, and optimized bytecode compilation.

[中文文档 (Chinese Documentation)](README_CN.md)

---

## Features

### Core Enhancements

- **Secure Compilation**: Dynamic OPcode mapping, timestamp encryption, SHA-256 integrity verification.
- **Custom VM**: Implements XCLUA instruction set with 64-bit instruction format and optimized dispatch.
- **Syntax Extensions**: Modern language features including Classes, Switch, Try-Catch, Arrow Functions, Pipe Operators, and more.
- **Shell-like Conditions**: Built-in support for shell-style test expressions (e.g., `[ -f "file.txt" ]`).
- **Code Obfuscation**: Control flow flattening, block shuffling, bogus blocks, VM protection, and string encryption.
- **JIT Compilation**: Built-in TCC (Tiny C Compiler) integration for runtime C code compilation.

### Extension Modules

| Module | Description |
|--------|-------------|
| `json` | Built-in JSON parsing/serialization |
| `lclass` | OOP support (classes, inheritance, interfaces) |
| `lbitlib` | Bitwise operations |
| `lboolib` | Boolean enhancements |
| `ludatalib` | Binary data serialization |
| `lsmgrlib` | Memory management utilities |
| `process` | Process management (Linux only) |
| `http` | HTTP client/server & Socket |
| `thread` | Multithreading support with mutex, condition variables, and read-write locks |
| `fs` | File system operations |
| `struct` | C-style structs & arrays |
| `ptr` | Pointer operations library |
| `vm` | VM introspection and bytecode manipulation |
| `tcc` | Runtime C code compilation via TCC |
| `ByteCode` | Bytecode manipulation and analysis |
| `vmprotect` | VM-based code protection |
| `translator` | Code translation utilities |

---

## Syntax Extensions

LXCLUA-NCore introduces modern language features to extend Lua 5.5.

### 1. Extended Operators

Supports compound assignments, increment/decrement, spaceship operator, null coalescing, optional chaining, pipe operators, and walrus operator.

```lua
-- Compound Assignment & Increment
local a = 10
a += 5          -- a = 15
a++             -- a = 16

-- Spaceship Operator (-1, 0, 1)
local cmp = 10 <=> 20  -- -1

-- Null Coalescing
local val = nil
local res = val ?? "default"  -- "default"

-- Optional Chaining
local config = { server = { port = 8080 } }
local port = config?.server?.port  -- 8080
local timeout = config?.client?.timeout  -- nil

-- Pipe Operator
local function double(x) return x * 2 end
local result = 10 |> double  -- 20

-- Safe Pipe (skips if nil)
local maybe_nil = nil
local _ = maybe_nil |?> print  -- (does nothing)

-- Walrus Operator (Assignment Expression)
local x
if (x := 100) > 50 then
    print(x) -- 100
end
```

### 2. Enhanced Strings

- **Interpolation**: `${var}` or `${[expr]}` inside strings.
- **Raw Strings**: Prefixed with `_raw`, ignores escape sequences.

```lua
local name = "World"
print("Hello, ${name}!")  -- Hello, World!

local calc = "1 + 1 = ${[1+1]}"  -- 1 + 1 = 2

local path = _raw"C:\Windows\System32"
```

### 3. Function Features

Supports arrow functions, lambdas, C-style definitions, generics, and async/await.

```lua
-- Arrow Function
local add = (a, b) => a + b
local log = ->(msg) { print("[LOG]: " .. msg) }

-- Lambda Expression
local sq = lambda(x): x * x

-- C-style Function
int sum(int a, int b) {
    return a + b;
}

-- Generic Function
local function Factory(T)(val)
    return { type = T, value = val }
end
local obj = Factory("int")(99)

-- Async/Await
async function fetchData(url)
    -- local data = await http.get(url) -- (Requires async runtime)
    return "data"
end
```

### 4. Object-Oriented Programming (OOP)

Complete class and interface system with modifiers (`private`, `public`, `protected`, `static`, `final`, `abstract`, `sealed`) and properties (`get`/`set`).

```lua
interface Drawable
    function draw(self)
end

class Shape implements Drawable
    function draw(self)
        -- abstract-like behavior
    end
end

-- Sealed Class (cannot be extended)
sealed class Circle extends Shape
    private _radius = 0
    protected _id = 0

    function __init__(self, r)
        self._radius = r
    end

    -- Property with Getter/Setter
    get radius(self)
        return self._radius
    end

    set radius(self, v)
        if v >= 0 then self._radius = v end
    end

    function draw(self)
        super.draw(self)
        return "Drawing circle: " .. self._radius
    end

    static function create(r)
        return new Circle(r)
    end
end

local c = Circle.create(10)
c.radius = 20
print(c.radius)  -- 20

-- instanceof check
if c instanceof Circle then
    print("c is a Circle")
end
```

### 5. Structs & Types

```lua
-- Struct
struct Point {
    int x;
    int y;
}
local p = Point()
p.x = 10

-- Concept (Type Predicate)
concept IsPositive(x)
    return x > 0
end
-- Or single expression form
concept IsEven(x) = x % 2 == 0

-- SuperStruct (Enhanced Table Definition)
superstruct MetaPoint [
    x: 0,
    y: 0,
    ["move"]: function(self, dx, dy)
        self.x = self.x + dx
        self.y = self.y + dy
    end
]

-- Enum
enum Color {
    Red,
    Green,
    Blue = 10
}

-- Destructuring
local data = { x = 1, y = 2 }
local take { x, y } = data
```

### 6. Control Flow

```lua
-- Switch Statement
switch (val) do
    case 1:
        print("One")
        break
    default:
        print("Other")
end

-- When Statement (Pattern Matching)
do
    when x == 1
        print("x is 1")
    case x == 10
        print("x is 10")
    else
        print("other")
end

-- Try-Catch-Finally
try
    error("Error")
catch(e)
    print("Caught: " .. e)
finally
    print("Cleanup")
end

-- Defer
defer do print("Executes at scope exit") end

-- With Statement
local ctx = { val = 10 }
with (ctx) {
    print(val) -- 10
}

-- Namespace & Using
namespace MyLib {
    function test() return "test" end
}
using namespace MyLib; -- Import all
-- using MyLib::test;  -- Import specific member
```

### 7. Shell-like Tests

Built-in conditional tests using `[ ... ]` syntax.

```lua
if [ -f "config.lua" ] then
    print("Config file exists")
end

if [ "a" == "a" ] then
    print("Strings match")
end

if [ 10 -gt 5 ] then
    print("10 > 5")
end
```

### 8. Metaprogramming & Macros

```lua
-- Custom Command
command echo(msg)
    print(msg)
end
echo "Hello World"

-- Custom Operator
operator ++ (x)
    return x + 1
end
-- Call with $$ prefix
local res = $$++(10)

-- Preprocessor Directives
$define DEBUG 1
$alias CONST_VAL = 100
$type MyInt = int

$if DEBUG
    print("Debug mode")
$else
    print("Release mode")
$end

$declare g_var: MyInt

-- Object Macro
local x = 10
local obj = $object(x) -- {x=10}
```

### 9. Inline ASM

Write VM instructions directly. Use `newreg` to allocate registers safely.
Supports pseudo-instructions like `rep`, `_if`, `_print`.

```lua
asm(
    newreg r0
    LOADI r0 100

    -- Compile-time loop
    rep 5 {
        ADDI r0 r0 1
    }

    -- Conditional assembly
    _if 1
       _print "Compiling this block"
    _endif

    -- Embedding data
    -- db 1, 2, 3, 4
    -- str "RawData"

    RETURN1 r0
)
```

### 10. Slice Operations

Python-style slice syntax for tables and strings.

```lua
local arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}

local slice1 = arr[1:5]      -- {1, 2, 3, 4, 5}
local slice2 = arr[1:10:2]   -- {1, 3, 5, 7, 9}
local slice3 = arr[5:]       -- {5, 6, 7, 8, 9, 10}
local slice4 = arr[:5]       -- {1, 2, 3, 4, 5}
local slice5 = arr[::-1]     -- {10, 9, 8, 7, 6, 5, 4, 3, 2, 1}
```

### 11. `in` Operator

Check if a value exists in a container.

```lua
local arr = {1, 2, 3, 4, 5}
if 3 in arr then
    print("3 is in arr")
end

local str = "Hello World"
if "World" in str then
    print("Found 'World'")
end
```

### 12. Type Hints

Support for type annotations and type checking.

```lua
local function greet(name: string): string
    return "Hello, " .. name
end

local x: int = 10
local y: float = 3.14
local flag: bool = true
```

---

## Security Features

### Code Obfuscation

LXCLUA-NCore provides multiple obfuscation techniques:

| Flag | Description |
|------|-------------|
| `OBFUSCATE_CFF` | Control Flow Flattening |
| `OBFUSCATE_BLOCK_SHUFFLE` | Randomize basic block order |
| `OBFUSCATE_BOGUS_BLOCKS` | Insert bogus basic blocks |
| `OBFUSCATE_STATE_ENCODE` | Obfuscate state variable values |
| `OBFUSCATE_NESTED_DISPATCHER` | Multi-layered dispatcher |
| `OBFUSCATE_OPAQUE_PREDICATES` | Opaque predicates |
| `OBFUSCATE_FUNC_INTERLEAVE` | Function interleaving |
| `OBFUSCATE_VM_PROTECT` | VM protection (custom instruction set) |
| `OBFUSCATE_BINARY_DISPATCHER` | Binary search dispatcher |
| `OBFUSCATE_RANDOM_NOP` | Insert random NOP instructions |
| `OBFUSCATE_STR_ENCRYPT` | String constant encryption |

```lua
-- Obfuscate bytecode
local obfuscated = string.dump(func, false, OBFUSCATE_CFF | OBFUSCATE_STR_ENCRYPT)
```

### AES Encryption

Built-in AES encryption support (ECB, CBC, CTR modes).

```lua
local aes = require("aes")
local key = "16-byte-key-1234"
local iv = "initial-vector-16"
local encrypted = aes.encrypt_cbc(plaintext, key, iv)
local decrypted = aes.decrypt_cbc(encrypted, key, iv)
```

### SHA-256 Hashing

```lua
local sha256 = require("sha256")
local hash = sha256.hash("Hello World")
```

---

## Threading Support

Full multithreading support with synchronization primitives.

```lua
local thread = require("thread")

-- Mutex
local m = thread.mutex()
m:lock()
-- critical section
m:unlock()

-- Condition Variable
local cond = thread.cond()
cond:wait(m)
cond:signal()
cond:broadcast()

-- Read-Write Lock
local rwlock = thread.rwlock()
rwlock:rdlock()  -- read lock
rwlock:wrlock()  -- write lock
rwlock:unlock()

-- Thread Creation
local t = thread.create(function()
    print("Running in thread")
end)
t:join()
```

---

## TCC Integration (JIT Compilation)

Compile and execute C code at runtime.

```lua
local tcc = require("tcc")

local code = [[
    int add(int a, int b) {
        return a + b;
    }
]]

local state = tcc.new()
state:compile(code)
local add = state:get_symbol("add")
print(add(1, 2))  -- 3
```

---

## Extended Types

LXCLUA-NCore extends Lua with additional types:

| Type | Description |
|------|-------------|
| `LUA_TSTRUCT` | C-style struct |
| `LUA_TPOINTER` | Raw pointer type |
| `LUA_TCONCEPT` | Type predicate concept |
| `LUA_TNAMESPACE` | Namespace type |
| `LUA_TSUPERSTRUCT` | Enhanced table definition |

---

## Big Integer Support

Arbitrary precision integer arithmetic.

```lua
local bigint = require("bigint")

local a = bigint.new("12345678901234567890")
local b = bigint.new("98765432109876543210")
local c = a + b
print(c:tostring())  -- 111111111011111111100
```

---

## HTTP & Networking

```lua
local http = require("http")

-- HTTP GET
local response = http.get("https://api.example.com/data")
print(response.body)

-- HTTP POST
local result = http.post("https://api.example.com/submit", {
    headers = { ["Content-Type"] = "application/json" },
    body = '{"key": "value"}'
})

-- Socket operations
local sock = http.socket()
sock:connect("example.com", 80)
sock:send("GET / HTTP/1.0\r\n\r\n")
local data = sock:recv(1024)
sock:close()
```

---

## File System Operations

```lua
local fs = require("fs")

-- File operations
local content = fs.read("file.txt")
fs.write("output.txt", "Hello World")
fs.append("log.txt", "New entry\n")

-- Directory operations
local files = fs.listdir("/path/to/dir")
fs.mkdir("/path/to/new/dir")
fs.rmdir("/path/to/dir")

-- Path utilities
local exists = fs.exists("file.txt")
local is_dir = fs.isdir("path")
local is_file = fs.isfile("file.txt")
local size = fs.size("file.txt")
```

---

## Bytecode Manipulation

```lua
local bytecode = require("ByteCode")

-- Dump function to bytecode
local bc = bytecode.dump(function() print("Hello") end)

-- Load bytecode back
local func = bytecode.load(bc)

-- Analyze bytecode
local info = bytecode.analyze(func)
print("Instructions:", info.num_instructions)
print("Constants:", info.num_constants)
```

---

## Build & Test

### Build

```bash
# Linux
make linux

# Windows (MinGW)
make mingw

# Android
make android
```

### Verification

Run the test suite to verify all features:

```bash
./lxclua tests/verify_docs_full.lua
./lxclua tests/test_parser_features.lua
./lxclua tests/test_advanced_parser.lua
```

---

## API Reference

### Object-Oriented API

```c
// Class creation and manipulation
void lua_newclass(lua_State *L, const char *name);
void lua_inherit(lua_State *L, int child_idx, int parent_idx);
void lua_newobject(lua_State *L, int class_idx, int nargs);
void lua_setmethod(lua_State *L, int class_idx, const char *name, int func_idx);
void lua_setstatic(lua_State *L, int class_idx, const char *name, int value_idx);
void lua_getprop(lua_State *L, int obj_idx, const char *key);
void lua_setprop(lua_State *L, int obj_idx, const char *key, int value_idx);
int  lua_instanceof(lua_State *L, int obj_idx, int class_idx);
void lua_implement(lua_State *L, int class_idx, int interface_idx);
```

### Obfuscation API

```c
int lua_dump_obfuscated(lua_State *L, lua_Writer writer, void *data,
                        int strip, int obfuscate_flags, unsigned int seed,
                        const char *log_path);
```

### Enhanced Memory API

```c
size_t lua_getmemoryusage(lua_State *L);
void   lua_gc_force(lua_State *L);
void   lua_table_iextend(lua_State *L, int idx, int n);
```

---

## License

[MIT License](LICENSE).
Lua original code Copyright © PUC-Rio.
