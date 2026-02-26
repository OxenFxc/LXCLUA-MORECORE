--[[
    LXCLUA-NCore Namespace 完整使用指南
    
    namespace 关键字用于创建命名空间，提供模块化代码组织方式
    运行方式: ./lxclua test_namespace_complete.lua
]]

print("=" .. string.rep("=", 60))
print("LXCLUA-NCore Namespace 完整使用指南")
print("=" .. string.rep("=", 60))

-- ============================================================
-- 第一部分: 基本语法
-- ============================================================
print("\n[1] 基本语法")
print("-" .. string.rep("-", 60))

-- 1.1 创建简单命名空间
print("\n1.1 创建简单命名空间")
namespace SimpleNS {
    int value = 100;
    string name = "SimpleNS";
}
print("  SimpleNS.value = " .. SimpleNS.value)
print("  SimpleNS.name = " .. SimpleNS.name)
assert(SimpleNS.value == 100)
assert(SimpleNS.name == "SimpleNS")
print("  ✓ 通过")

-- 1.2 命名空间中的函数
print("\n1.2 命名空间中的函数")
namespace MathUtils {
    int add(int a, int b) {
        return a + b;
    }
    
    int multiply(int a, int b) {
        return a * b;
    }
    
    function greet(name)
        return "Hello, " .. name .. "!"
    end
}
print("  MathUtils.add(10, 20) = " .. MathUtils.add(10, 20))
print("  MathUtils.multiply(5, 6) = " .. MathUtils.multiply(5, 6))
print("  MathUtils.greet('World') = " .. MathUtils.greet("World"))
assert(MathUtils.add(10, 20) == 30)
assert(MathUtils.multiply(5, 6) == 30)
print("  ✓ 通过")

-- ============================================================
-- 第二部分: 访问方式
-- ============================================================
print("\n[2] 访问方式")
print("-" .. string.rep("-", 60))

-- 2.1 点号访问 (.)
print("\n2.1 点号访问 (.)")
namespace DotAccess {
    int x = 42;
}
print("  DotAccess.x = " .. DotAccess.x)
assert(DotAccess.x == 42)
print("  ✓ 通过")

-- 2.2 双冒号访问 (::)
print("\n2.2 双冒号访问 (::)")
namespace ColonAccess {
    int y = 99;
}
print("  ColonAccess::y = " .. ColonAccess::y)
assert(ColonAccess::y == 99)
print("  ✓ 通过")

-- 2.3 通过双冒号修改值
print("\n2.3 通过::修改值")
namespace Modifiable {
    int counter = 0;
}
print("  初始值: Modifiable.counter = " .. Modifiable.counter)
Modifiable::counter = 100
print("  修改后: Modifiable::counter = 100")
print("  验证: Modifiable.counter = " .. Modifiable.counter)
assert(Modifiable.counter == 100)
print("  ✓ 通过")

-- 2.4 链式访问
print("\n2.4 链式访问")
namespace OuterNS {
    int outer_val = 1;
    
    namespace InnerNS {
        int inner_val = 2;
        
        namespace DeepNS {
            int deep_val = 3;
        }
    }
}
print("  OuterNS.outer_val = " .. OuterNS.outer_val)
print("  OuterNS.InnerNS.inner_val = " .. OuterNS.InnerNS.inner_val)
print("  OuterNS::InnerNS::DeepNS::deep_val = " .. OuterNS::InnerNS::DeepNS::deep_val)
assert(OuterNS.outer_val == 1)
assert(OuterNS.InnerNS.inner_val == 2)
assert(OuterNS::InnerNS::DeepNS::deep_val == 3)
print("  ✓ 通过")

-- ============================================================
-- 第三部分: using 关键字
-- ============================================================
print("\n[3] using 关键字")
print("-" .. string.rep("-", 60))

-- 3.1 using namespace - 导入整个命名空间
print("\n3.1 using namespace 导入整个命名空间")
namespace ImportNS {
    int imported_var = 500;
    string message = "Imported!";
}
using namespace ImportNS;
print("  直接访问 imported_var = " .. imported_var)
print("  直接访问 message = " .. message)
assert(imported_var == 500)
assert(message == "Imported!")
print("  ✓ 通过")

-- 3.2 using Name::Member - 导入单个成员
print("\n3.2 using Name::Member 导入单个成员")
namespace SingleMember {
    int solo = 777;
    int other = 888;
}
using SingleMember::solo;
print("  导入后直接访问 solo = " .. solo)
assert(solo == 777)
print("  ✓ 通过")

-- 3.3 using 链式导入
print("\n3.3 using 链式导入")
namespace ChainNS {
    namespace SubNS {
        int chain_val = 999;
    }
}
using ChainNS::SubNS::chain_val;
print("  链式导入后 chain_val = " .. chain_val)
assert(chain_val == 999)
print("  ✓ 通过")

-- ============================================================
-- 第四部分: 命名空间中的类型定义
-- ============================================================
print("\n[4] 命名空间中的类型定义")
print("-" .. string.rep("-", 60))

-- 4.1 命名空间中的 class
print("\n4.1 命名空间中的 class")
namespace ClassNS {
    class Point {
        public int x = 0;
        public int y = 0;
        
        function __init__(self, x, y)
            self.x = x
            self.y = y
        end
        
        function show(self)
            return "Point(" .. self.x .. ", " .. self.y .. ")"
        end
    }
}
local pt = ClassNS.Point(10, 20)
print("  ClassNS.Point(10, 20):show() = " .. pt:show())
assert(pt.x == 10 and pt.y == 20)
print("  ✓ 通过")

-- 4.2 命名空间中的 struct
print("\n4.2 命名空间中的 struct")
namespace StructNS {
    struct Data {
        int id;
        string name;
    }
}
local data = StructNS.Data{id = 1, name = "test"}
print("  StructNS.Data{id=1, name='test'}")
print("  data.id = " .. data.id .. ", data.name = " .. data.name)
assert(data.id == 1)
assert(data.name == "test")
print("  ✓ 通过")

-- 4.3 命名空间中的 enum
print("\n4.3 命名空间中的 enum")
namespace EnumNS {
    enum Status {
        OK,
        ERROR,
        PENDING = 10
    }
}
print("  EnumNS.Status.OK = " .. EnumNS.Status.OK)
print("  EnumNS.Status.ERROR = " .. EnumNS.Status.ERROR)
print("  EnumNS.Status.PENDING = " .. EnumNS.Status.PENDING)
assert(EnumNS.Status.OK == 0)
assert(EnumNS.Status.ERROR == 1)
assert(EnumNS.Status.PENDING == 10)
print("  ✓ 通过")

-- 4.4 命名空间中的 interface
print("\n4.4 命名空间中的 interface")
namespace InterfaceNS {
    interface IShape
        function getArea(self)
    end
    
    class Rectangle implements IShape {
        private int _width = 0;
        private int _height = 0;
        
        function __init__(self, w, h)
            self._width = w
            self._height = h
        end
        
        function getArea(self)
            return self._width * self._height
        end
    }
}
local rect = InterfaceNS.Rectangle(5, 10)
print("  Rectangle(5, 10):getArea() = " .. rect:getArea())
assert(rect:getArea() == 50)
print("  ✓ 通过")

-- 4.5 命名空间中的 concept
print("\n4.5 命名空间中的 concept")
namespace ConceptNS {
    concept IsEven(x)
        return x % 2 == 0
    end
    
    concept IsPositive(x) = x > 0
}
print("  IsEven(4) = " .. tostring(ConceptNS.IsEven(4)))
print("  IsEven(5) = " .. tostring(ConceptNS.IsEven(5)))
print("  IsPositive(10) = " .. tostring(ConceptNS.IsPositive(10)))
assert(ConceptNS.IsEven(4) == true)
assert(ConceptNS.IsEven(5) == false)
assert(ConceptNS.IsPositive(10) == true)
print("  ✓ 通过")

-- ============================================================
-- 第五部分: 命名空间参数
-- ============================================================
print("\n[5] 命名空间参数")
print("-" .. string.rep("-", 60))

-- 5.1 带参数的命名空间
print("\n5.1 带参数的命名空间")
local external_var = 1000
local external_func = function(x) return x * 2 end

namespace ParamNS(external_var, external_func) {
    int captured = external_var;
    
    function double(x)
        return external_func(x)
    end
}
print("  ParamNS.captured = " .. ParamNS.captured)
print("  ParamNS.double(50) = " .. ParamNS.double(50))
assert(ParamNS.captured == 1000)
assert(ParamNS.double(50) == 100)
print("  ✓ 通过")

-- ============================================================
-- 第六部分: 嵌套命名空间
-- ============================================================
print("\n[6] 嵌套命名空间")
print("-" .. string.rep("-", 60))

-- 6.1 多层嵌套
print("\n6.1 多层嵌套")
namespace Level1 {
    int l1 = 1;
    
    namespace Level2 {
        int l2 = 2;
        
        namespace Level3 {
            int l3 = 3;
            
            function sum()
                return Level1.l1 + Level2.l2 + l3
            end
        }
    }
}
print("  Level1.l1 = " .. Level1.l1)
print("  Level1.Level2.l2 = " .. Level1.Level2.l2)
print("  Level1::Level2::Level3.l3 = " .. Level1::Level2::Level3.l3)
print("  Level1::Level2::Level3:sum() = " .. Level1::Level2::Level3:sum())
assert(Level1::Level2::Level3:sum() == 6)
print("  ✓ 通过")

-- ============================================================
-- 第七部分: 函数内的命名空间
-- ============================================================
print("\n[7] 函数内的命名空间")
print("-" .. string.rep("-", 60))

-- 7.1 在函数内使用 using namespace
print("\n7.1 函数内使用 using namespace")
namespace FuncNS {
    int func_var = 12345;
}

function test_func_namespace()
    using namespace FuncNS;
    return func_var * 2
end

local result = test_func_namespace()
print("  test_func_namespace() = " .. result)
assert(result == 24690)
print("  ✓ 通过")

-- 7.2 在函数内定义命名空间
print("\n7.2 函数内定义命名空间 (不推荐，但可行)")
function define_ns_in_func()
    namespace LocalNS {
        int val = 999;
    }
    return LocalNS.val
end
print("  define_ns_in_func() = " .. define_ns_in_func())
print("  ✓ 通过")

-- ============================================================
-- 第八部分: 命名空间与全局变量
-- ============================================================
print("\n[8] 命名空间与全局变量")
print("-" .. string.rep("-", 60))

-- 8.1 命名空间作为全局变量
print("\n8.1 命名空间作为全局变量")
namespace GlobalNS {
    int global_val = 111;
}
print("  _G.GlobalNS 存在: " .. tostring(_G.GlobalNS ~= nil))
print("  _G.GlobalNS.global_val = " .. _G.GlobalNS.global_val)
assert(_G.GlobalNS.global_val == 111)
print("  ✓ 通过")

-- 8.2 从命名空间外部访问
print("\n8.2 从命名空间外部访问和修改")
GlobalNS.global_val = 222
print("  修改后 GlobalNS.global_val = " .. GlobalNS.global_val)
assert(GlobalNS.global_val == 222)
print("  ✓ 通过")

-- ============================================================
-- 第九部分: 实际应用场景
-- ============================================================
print("\n[9] 实际应用场景")
print("-" .. string.rep("-", 60))

-- 9.1 模块化配置
print("\n9.1 模块化配置")
namespace Config {
    namespace Database {
        string host = "localhost";
        int port = 3306;
        string name = "mydb";
    }
    
    namespace Server {
        string host = "0.0.0.0";
        int port = 8080;
    }
}
print("  Database: " .. Config.Database.host .. ":" .. Config.Database.port)
print("  Server: " .. Config.Server.host .. ":" .. Config.Server.port)
print("  ✓ 通过")

-- 9.2 工具函数库
print("\n9.2 工具函数库")
namespace Utils {
    namespace String {
        function trim(s)
            return s:match("^%s*(.-)%s*$")
        end
        
        function split(s, sep)
            local result = {}
            for part in s:gmatch("[^" .. sep .. "]+") do
                table.insert(result, part)
            end
            return result
        end
    }
    
    namespace Math {
        function clamp(val, min, max)
            if val < min then return min end
            if val > max then return max end
            return val
        end
        
        function lerp(a, b, t)
            return a + (b - a) * t
        end
    }
}
print("  Utils.String.trim('  hello  ') = '" .. Utils.String.trim("  hello  ") .. "'")
local parts = Utils.String.split("a,b,c", ",")
print("  Utils.String.split('a,b,c', ',') = {" .. table.concat(parts, ", ") .. "}")
print("  Utils.Math.clamp(150, 0, 100) = " .. Utils.Math.clamp(150, 0, 100))
print("  Utils.Math.lerp(0, 100, 0.5) = " .. Utils.Math.lerp(0, 100, 0.5))
assert(Utils.String.trim("  hello  ") == "hello")
assert(Utils.Math.clamp(150, 0, 100) == 100)
assert(Utils.Math.lerp(0, 100, 0.5) == 50)
print("  ✓ 通过")

-- 9.3 游戏实体系统
print("\n9.3 游戏实体系统示例")
namespace Game {
    namespace Entities {
        class Entity {
            public string name = "Entity";
            public int x = 0;
            public int y = 0;
            
            function __init__(self, name, x, y)
                self.name = name
                self.x = x
                self.y = y
            end
            
            function move(self, dx, dy)
                self.x = self.x + dx
                self.y = self.y + dy
            end
        }
        
        class Player extends Entity {
            public int health = 100;
            
            function __init__(self, name, x, y)
                super(name, x, y)
            end
            
            function takeDamage(self, amount)
                self.health = self.health - amount
            end
        }
    }
    
    namespace Constants {
        int MAX_HEALTH = 100;
        int DEFAULT_SPEED = 5;
    }
}
local player = Game.Entities.Player("Hero", 0, 0)
player:move(10, 20)
player:takeDamage(30)
print("  Player: " .. player.name .. " at (" .. player.x .. ", " .. player.y .. ")")
print("  Health: " .. player.health .. "/" .. Game.Constants.MAX_HEALTH)
assert(player.x == 10 and player.y == 20)
assert(player.health == 70)
print("  ✓ 通过")

-- ============================================================
-- 第十部分: 高级特性
-- ============================================================
print("\n[10] 高级特性")
print("-" .. string.rep("-", 60))

-- 10.1 命名空间中的静态方法
print("\n10.1 命名空间中的静态方法")
namespace StaticNS {
    class Factory {
        static function create(type_name)
            return { type = type_name, created = os.time() }
        end
    }
}
local obj = StaticNS.Factory.create("Widget")
print("  Factory.create('Widget') -> type = " .. obj.type)
assert(obj.type == "Widget")
print("  ✓ 通过")

-- 10.2 命名空间中的 superstruct
print("\n10.2 命名空间中的 superstruct")
namespace SuperNS {
    superstruct MetaData [
        version: "1.0",
        author: "Developer",
        ["getDescription"]: function(self)
            return self.version .. " by " .. self.author
        end
    ]
}
print("  MetaData.version = " .. SuperNS.MetaData.version)
print("  MetaData:getDescription() = " .. SuperNS.MetaData:getDescription())
assert(SuperNS.MetaData.version == "1.0")
print("  ✓ 通过")

-- ============================================================
-- 测试总结
-- ============================================================
print("\n" .. "=" .. string.rep("=", 60))
print("所有测试完成!")
print("=" .. string.rep("=", 60))

print([[

    ================================================================
    Namespace 语法速查表
    ================================================================
    
    【基本语法】
    namespace Name {
        -- 成员定义
    }
    
    【成员类型】
    - 变量: int x = 10;
    - 函数: function foo() ... end
    - C风格函数: int add(int a, int b) { return a + b; }
    - 类: class MyClass { ... }
    - 结构体: struct MyStruct { int x; int y; }
    - 枚举: enum MyEnum { A, B, C }
    - 接口: interface IMyInterface { ... }
    - 概念: concept MyConcept(x) ... end
    - 超结构: superstruct MySuper [ ... ]
    - 嵌套命名空间: namespace Inner { ... }
    
    【访问方式】
    - 点号访问: Name.member
    - 双冒号访问: Name::member
    - 链式访问: Name::SubName::member
    
    【using 导入】
    - 导入整个命名空间: using namespace Name;
    - 导入单个成员: using Name::member;
    - 链式导入: using Name::SubName::member;
    
    【带参数的命名空间】
    namespace Name(var1, var2) {
        -- 可以使用 var1, var2
    }
    
    【嵌套命名空间】
    namespace Outer {
        namespace Inner {
            -- 成员
        }
    }
    访问: Outer.Inner.member 或 Outer::Inner::member
    
    【注意事项】
    1. 命名空间会创建全局变量
    2. using namespace 会链接到当前环境
    3. 命名空间内部可以直接访问其他成员
    4. 支持在函数内使用 using namespace
    
    ================================================================
]])
