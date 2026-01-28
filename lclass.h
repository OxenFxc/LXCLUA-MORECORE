/*
** $Id: lclass.h $
** Lua 面向对象系统 - 类与对象支持
** 提供原生的类定义、继承、实例化等面向对象功能
** See Copyright Notice in lua.h
*/

#ifndef lclass_h
#define lclass_h

#include "lobject.h"
#include "lstate.h"
#include "ltable.h"

/*
** =====================================================================
** 类系统核心结构定义
** =====================================================================
*/

/*
** 类的标志位定义
*/
#define CLASS_FLAG_FINAL      (1 << 0)   /* 类不可被继承 */
#define CLASS_FLAG_ABSTRACT   (1 << 1)   /* 抽象类，不可直接实例化 */
#define CLASS_FLAG_INTERFACE  (1 << 2)   /* 接口类型 */
#define CLASS_FLAG_SEALED     (1 << 3)   /* 密封类 */

/*
** 成员访问控制级别
*/
#define ACCESS_PUBLIC     0   /* 公开成员 */
#define ACCESS_PROTECTED  1   /* 受保护成员（子类可访问） */
#define ACCESS_PRIVATE    2   /* 私有成员（仅本类可访问） */

/*
** 成员类型标志
*/
#define MEMBER_METHOD     (1 << 0)   /* 方法 */
#define MEMBER_FIELD      (1 << 1)   /* 字段 */
#define MEMBER_STATIC     (1 << 2)   /* 静态成员 */
#define MEMBER_CONST      (1 << 3)   /* 常量成员 */
#define MEMBER_VIRTUAL    (1 << 4)   /* 虚方法（可被重写） */
#define MEMBER_OVERRIDE   (1 << 5)   /* 重写父类方法 */
#define MEMBER_ABSTRACT   (1 << 6)   /* 抽象方法（必须被子类实现） */
#define MEMBER_FINAL      (1 << 7)   /* final方法（不可被重写） */

/*
** 类元信息键名
** 这些键名用于存储类的元信息
*/
#define CLASS_KEY_NAME       "__classname"    /* 类名 */
#define CLASS_KEY_PARENT     "__parent"       /* 父类引用 */
#define CLASS_KEY_METHODS    "__methods"      /* 方法表（公开方法） */
#define CLASS_KEY_STATICS    "__statics"      /* 静态成员表 */
#define CLASS_KEY_PRIVATES   "__privates"     /* 私有成员表 */
#define CLASS_KEY_PROTECTED  "__protected"    /* 受保护成员表 */
#define CLASS_KEY_INIT       "__init__"       /* 构造函数 */
#define CLASS_KEY_DESTRUCTOR "__gc"           /* 析构函数 */
#define CLASS_KEY_ISCLASS    "__isclass"      /* 标记这是一个类 */
#define CLASS_KEY_INTERFACES "__interfaces"   /* 实现的接口列表 */
#define CLASS_KEY_FLAGS      "__flags"        /* 类标志 */
#define CLASS_KEY_ABSTRACTS  "__abstracts"    /* 抽象方法表 */
#define CLASS_KEY_FINALS     "__finals"       /* final方法表 */
#define CLASS_KEY_GETTERS    "__getters"      /* getter方法表（公开） */
#define CLASS_KEY_SETTERS    "__setters"      /* setter方法表（公开） */
#define CLASS_KEY_PRIVATE_GETTERS   "__private_getters"   /* 私有getter方法表 */
#define CLASS_KEY_PRIVATE_SETTERS   "__private_setters"   /* 私有setter方法表 */
#define CLASS_KEY_PROTECTED_GETTERS "__protected_getters" /* 受保护getter方法表 */
#define CLASS_KEY_PROTECTED_SETTERS "__protected_setters" /* 受保护setter方法表 */
#define CLASS_KEY_MEMBER_FLAGS "__member_flags" /* 成员标志表 */

/*
** 对象元信息键名
*/
#define OBJ_KEY_CLASS        "__class"        /* 对象所属的类 */
#define OBJ_KEY_ISOBJ        "__isobject"     /* 标记这是一个对象实例 */
#define OBJ_KEY_PRIVATES     "__obj_privates" /* 对象私有数据 */

/*
** =====================================================================
** 类系统核心函数声明
** =====================================================================
*/

/*
** 创建新类
** 参数：
**   L - Lua状态机
**   name - 类名（字符串）
** 返回值：
**   返回创建的类表（压入栈顶）
** 说明：
**   创建一个新的类表，设置必要的元表和元信息
*/
LUAI_FUNC void luaC_newclass(lua_State *L, TString *name);

/*
** 设置类的继承关系
** 参数：
**   L - Lua状态机
**   child_idx - 子类在栈中的索引
**   parent_idx - 父类在栈中的索引
** 说明：
**   设置子类继承自父类，复制父类的方法到子类
*/
LUAI_FUNC void luaC_inherit(lua_State *L, int child_idx, int parent_idx);

/*
** 创建类的实例对象
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   nargs - 构造函数参数个数
** 返回值：
**   返回创建的对象实例（压入栈顶）
** 说明：
**   创建新对象，设置元表，调用构造函数
*/
LUAI_FUNC void luaC_newobject(lua_State *L, int class_idx, int nargs);

/*
** 调用父类方法
** 参数：
**   L - Lua状态机
**   obj_idx - 对象在栈中的索引
**   method - 方法名
** 说明：
**   从父类中查找并调用指定方法
*/
LUAI_FUNC void luaC_super(lua_State *L, int obj_idx, TString *method);

/*
** 设置类方法
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 方法名
**   func_idx - 函数在栈中的索引
** 说明：
**   将函数设置为类的方法
*/
LUAI_FUNC void luaC_setmethod(lua_State *L, int class_idx, TString *name, 
                               int func_idx);

/*
** 设置静态成员
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 成员名
**   value_idx - 值在栈中的索引
** 说明：
**   设置类的静态成员
*/
LUAI_FUNC void luaC_setstatic(lua_State *L, int class_idx, TString *name,
                               int value_idx);

/*
** 获取属性（考虑继承链）
** 参数：
**   L - Lua状态机
**   obj_idx - 对象在栈中的索引
**   key - 属性名
** 返回值：
**   属性值压入栈顶，如果未找到则压入nil
** 说明：
**   从对象本身和继承链中查找属性
*/
LUAI_FUNC void luaC_getprop(lua_State *L, int obj_idx, TString *key);

/*
** 设置属性
** 参数：
**   L - Lua状态机
**   obj_idx - 对象在栈中的索引
**   key - 属性名
**   value_idx - 值在栈中的索引
** 说明：
**   设置对象的属性
*/
LUAI_FUNC void luaC_setprop(lua_State *L, int obj_idx, TString *key,
                             int value_idx);

/*
** 检查对象是否是指定类的实例
** 参数：
**   L - Lua状态机
**   obj_idx - 对象在栈中的索引
**   class_idx - 类在栈中的索引
** 返回值：
**   1 - 是该类的实例
**   0 - 不是该类的实例
** 说明：
**   检查对象是否是该类或其子类的实例
*/
LUAI_FUNC int luaC_instanceof(lua_State *L, int obj_idx, int class_idx);

/*
** 检查值是否是一个类
** 参数：
**   L - Lua状态机
**   idx - 值在栈中的索引
** 返回值：
**   1 - 是类
**   0 - 不是类
*/
LUAI_FUNC int luaC_isclass(lua_State *L, int idx);

/*
** 检查值是否是一个对象实例
** 参数：
**   L - Lua状态机
**   idx - 值在栈中的索引
** 返回值：
**   1 - 是对象
**   0 - 不是对象
*/
LUAI_FUNC int luaC_isobject(lua_State *L, int idx);

/*
** 获取对象所属的类
** 参数：
**   L - Lua状态机
**   obj_idx - 对象在栈中的索引
** 返回值：
**   类表压入栈顶，如果不是对象则压入nil
*/
LUAI_FUNC void luaC_getclass(lua_State *L, int obj_idx);

/*
** 获取类的父类
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
** 返回值：
**   父类压入栈顶，如果没有父类则压入nil
*/
LUAI_FUNC void luaC_getparent(lua_State *L, int class_idx);

/*
** 获取类名
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
** 返回值：
**   类名字符串
*/
LUAI_FUNC const char *luaC_classname(lua_State *L, int class_idx);

/*
** =====================================================================
** 接口相关函数
** =====================================================================
*/

/*
** 创建接口
** 参数：
**   L - Lua状态机
**   name - 接口名
** 返回值：
**   接口表压入栈顶
*/
LUAI_FUNC void luaC_newinterface(lua_State *L, TString *name);

/*
** 实现接口
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   interface_idx - 接口在栈中的索引
** 说明：
**   标记类实现指定接口
*/
LUAI_FUNC void luaC_implement(lua_State *L, int class_idx, int interface_idx);

/*
** 检查类是否实现了接口
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   interface_idx - 接口在栈中的索引
** 返回值：
**   1 - 实现了接口
**   0 - 未实现接口
*/
LUAI_FUNC int luaC_implements(lua_State *L, int class_idx, int interface_idx);

/*
** =====================================================================
** 初始化函数
** =====================================================================
*/

/*
** 初始化类系统
** 参数：
**   L - Lua状态机
** 说明：
**   初始化类系统所需的全局数据
*/
LUAI_FUNC void luaC_initclass(lua_State *L);

/*
** =====================================================================
** 访问控制相关函数
** =====================================================================
*/

/*
** 设置私有成员
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 成员名
**   value_idx - 值在栈中的索引
** 说明：
**   将成员设置为私有，只有本类内部可以访问
*/
LUAI_FUNC void luaC_setprivate(lua_State *L, int class_idx, TString *name,
                                int value_idx);

/*
** 设置受保护成员
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 成员名
**   value_idx - 值在栈中的索引
** 说明：
**   将成员设置为受保护，本类和子类可以访问
*/
LUAI_FUNC void luaC_setprotected(lua_State *L, int class_idx, TString *name,
                                  int value_idx);

/*
** 检查访问权限
** 参数：
**   L - Lua状态机
**   obj_idx - 对象在栈中的索引
**   key - 要访问的成员名
**   caller_class_idx - 调用者所属类的索引（0表示外部调用）
** 返回值：
**   ACCESS_PUBLIC - 可以公开访问
**   ACCESS_PROTECTED - 需要子类关系才能访问
**   ACCESS_PRIVATE - 需要同类才能访问
**   -1 - 成员不存在
*/
LUAI_FUNC int luaC_checkaccess(lua_State *L, int obj_idx, TString *key,
                                int caller_class_idx);

/*
** 检查类是否是另一个类的子类
** 参数：
**   L - Lua状态机
**   child_idx - 可能的子类在栈中的索引
**   parent_idx - 可能的父类在栈中的索引
** 返回值：
**   1 - 是子类（或同一个类）
**   0 - 不是子类
*/
LUAI_FUNC int luaC_issubclass(lua_State *L, int child_idx, int parent_idx);

/*
** =====================================================================
** 抽象方法和final方法相关函数
** =====================================================================
*/

/*
** 声明抽象方法
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 方法名
**   nparams - 方法期望的参数个数（用于验证实现类的方法签名）
** 说明：
**   声明一个抽象方法，子类必须实现该方法
*/
LUAI_FUNC void luaC_setabstract(lua_State *L, int class_idx, TString *name, int nparams);

/*
** 设置final方法
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 方法名
**   func_idx - 函数在栈中的索引
** 说明：
**   设置一个final方法，子类不能重写该方法
*/
LUAI_FUNC void luaC_setfinal(lua_State *L, int class_idx, TString *name, 
                              int func_idx);

/*
** 验证抽象方法是否都被实现
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
** 返回值：
**   1 - 所有抽象方法都已实现
**   0 - 存在未实现的抽象方法（会产生错误）
** 说明：
**   检查类是否实现了所有继承的抽象方法，并验证参数数量
*/
LUAI_FUNC int luaC_verify_abstracts(lua_State *L, int class_idx);

/*
** 验证接口方法是否都被正确实现
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
** 返回值：
**   1 - 所有接口方法都已正确实现
**   0 - 存在未实现或参数不匹配的接口方法（会产生错误）
** 说明：
**   检查类是否实现了所有接口声明的方法，并验证参数数量
*/
LUAI_FUNC int luaC_verify_interfaces(lua_State *L, int class_idx);

/*
** 检查方法是否可以被重写
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 方法名
** 返回值：
**   1 - 可以重写
**   0 - 不能重写（是final方法）
*/
LUAI_FUNC int luaC_can_override(lua_State *L, int class_idx, TString *name);

/*
** =====================================================================
** getter/setter属性访问器相关函数
** =====================================================================
*/

/*
** 设置getter方法
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   prop_name - 属性名
**   func_idx - getter函数在栈中的索引
**   access_level - 访问级别（ACCESS_PUBLIC/PROTECTED/PRIVATE）
** 说明：
**   当访问指定属性时，会调用getter函数
**   根据访问级别存储到不同的getter表中
*/
LUAI_FUNC void luaC_setgetter(lua_State *L, int class_idx, TString *prop_name,
                               int func_idx, int access_level);

/*
** 设置setter方法
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   prop_name - 属性名
**   func_idx - setter函数在栈中的索引
**   access_level - 访问级别（ACCESS_PUBLIC/PROTECTED/PRIVATE）
** 说明：
**   当设置指定属性时，会调用setter函数
**   根据访问级别存储到不同的setter表中
*/
LUAI_FUNC void luaC_setsetter(lua_State *L, int class_idx, TString *prop_name,
                               int func_idx, int access_level);

/*
** 设置成员标志
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 成员名
**   flags - 标志位（MEMBER_*）
*/
LUAI_FUNC void luaC_setmemberflags(lua_State *L, int class_idx, TString *name,
                                    int flags);

/*
** 获取成员标志
** 参数：
**   L - Lua状态机
**   class_idx - 类在栈中的索引
**   name - 成员名
** 返回值：
**   成员标志位，不存在返回0
*/
LUAI_FUNC int luaC_getmemberflags(lua_State *L, int class_idx, TString *name);

#endif
