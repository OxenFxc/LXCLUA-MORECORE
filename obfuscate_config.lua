--[[
** obfuscate_config.lua **
** 混淆配置工具 **

该脚本用于配置Lua字节码混淆选项并计算对应的flag值。

用法：
1. 修改下方的config表，将需要启用的选项设置为true
2. 运行该脚本：lua obfuscate_config.lua
3. 脚本将计算并显示对应的混淆标志值
4. 也可以在其他脚本中require该文件，使用calculateFlag函数

混淆选项说明：
- CFF: 控制流扁平化 - 将原始控制流转换为统一的dispatcher-switch结构
- BLOCK_SHUFFLE: 基本块随机打乱顺序 - 增加静态分析难度
- BOGUS_BLOCKS: 插入虚假基本块 - 增加代码复杂度
- STATE_ENCODE: 状态值编码混淆 - 对状态变量进行编码
- NESTED_DISPATCHER: 嵌套分发器 - 使用多层状态机
- OPAQUE_PREDICATES: 不透明谓词 - 插入恒真/恒假条件
- FUNC_INTERLEAVE: 函数交织 - 创建虚假函数路径
- VM_PROTECT: VM保护 - 使用自定义虚拟机指令集
]]

-- 混淆标志位定义
local ObfuscateFlags = {
    NONE = 0,
    CFF = 1 << 0,           -- 控制流扁平化
    BLOCK_SHUFFLE = 1 << 1, -- 基本块随机打乱顺序
    BOGUS_BLOCKS = 1 << 2,  -- 插入虚假基本块
    STATE_ENCODE = 1 << 3,  -- 状态值编码混淆
    NESTED_DISPATCHER = 1 << 4, -- 嵌套分发器（多层状态机）
    OPAQUE_PREDICATES = 1 << 5, -- 不透明谓词（恒真/恒假条件）
    FUNC_INTERLEAVE = 1 << 6, -- 函数交织（虚假函数路径）
    VM_PROTECT = 1 << 7     -- VM保护（自定义虚拟机指令集）
}

-- 选项描述
local OptionDescriptions = {
    CFF = "控制流扁平化",
    BLOCK_SHUFFLE = "基本块随机打乱顺序",
    BOGUS_BLOCKS = "插入虚假基本块",
    STATE_ENCODE = "状态值编码混淆",
    NESTED_DISPATCHER = "嵌套分发器",
    OPAQUE_PREDICATES = "不透明谓词",
    FUNC_INTERLEAVE = "函数交织",
    VM_PROTECT = "VM保护"
}

-- ======================
-- 配置表（用户修改这里）
-- ======================
local config = {
    CFF = true,           -- 控制流扁平化
    BLOCK_SHUFFLE = true, -- 基本块随机打乱顺序
    BOGUS_BLOCKS = false,  -- 插入虚假基本块
    STATE_ENCODE = true,  -- 状态值编码混淆
    NESTED_DISPATCHER = false, -- 嵌套分发器（多层状态机）
    OPAQUE_PREDICATES = true, -- 不透明谓词（恒真/恒假条件）
    FUNC_INTERLEAVE = false, -- 函数交织（虚假函数路径）
    VM_PROTECT = false     -- VM保护（自定义虚拟机指令集）
}

-- 计算混淆标志
-- @param options 布尔值表，包含各个选项的启用状态
-- @return 计算得到的混淆标志值
local function calculateFlag(options)
    local flag = ObfuscateFlags.NONE
    
    if options.CFF then
        flag = flag | ObfuscateFlags.CFF
    end
    if options.BLOCK_SHUFFLE then
        flag = flag | ObfuscateFlags.BLOCK_SHUFFLE
    end
    if options.BOGUS_BLOCKS then
        flag = flag | ObfuscateFlags.BOGUS_BLOCKS
    end
    if options.STATE_ENCODE then
        flag = flag | ObfuscateFlags.STATE_ENCODE
    end
    if options.NESTED_DISPATCHER then
        flag = flag | ObfuscateFlags.NESTED_DISPATCHER
    end
    if options.OPAQUE_PREDICATES then
        flag = flag | ObfuscateFlags.OPAQUE_PREDICATES
    end
    if options.FUNC_INTERLEAVE then
        flag = flag | ObfuscateFlags.FUNC_INTERLEAVE
    end
    if options.VM_PROTECT then
        flag = flag | ObfuscateFlags.VM_PROTECT
    end
    
    return flag
end

-- 显示结果
local function displayResult(flag, options)
    print("====================================")
    print("            配置结果")
    print("====================================")
    print("启用的选项:")
    for option, enabled in pairs(options) do
        if enabled then
            print(string.format("- %s", OptionDescriptions[option]))
        end
    end
    print()
    print(string.format("计算得到的混淆标志值: 0x%X (十进制: %d)", flag, flag))
    print()
    print("使用示例:")
    print(string.format("luaO_flatten(L, proto, 0x%X, seed, log_path);", flag))
    print()
end

-- 主函数
local function main()
    -- 计算标志
    local flag = calculateFlag(config)
    
    -- 显示结果
    displayResult(flag, config)
end

-- 导出函数
if not ... then
    -- 直接运行脚本
    main()
else
    -- 作为模块被require
    return {
        ObfuscateFlags = ObfuscateFlags,
        OptionDescriptions = OptionDescriptions,
        calculateFlag = calculateFlag,
        config = config
    }
end
