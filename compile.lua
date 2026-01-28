-- Lua 字节码编译工具
-- 支持 Windows 和 Android 路径

local function compile(input_path, output_path, strip)
    -- 默认不 strip 调试信息
    if strip == nil then strip = false end
    
    -- 读取源文件
    local f, err = io.open(input_path, "rb")
    if not f then
        return nil, "无法打开输入文件: " .. (err or input_path)
    end
    local source = f:read("*a")
    f:close()
    
    -- 编译为函数
    local chunk, err = load(source, "@" .. input_path)
    if not chunk then
        return nil, "编译失败: " .. (err or "未知错误")
    end
    
    -- 导出字节码
    local bytecode = string.dump(chunk, strip)
    
    -- 写入输出文件
    local out, err = io.open(output_path, "wb")
    if not out then
        return nil, "无法打开输出文件: " .. (err or output_path)
    end
    out:write(bytecode)
    out:close()
    
    return true, #bytecode
end

return compile
