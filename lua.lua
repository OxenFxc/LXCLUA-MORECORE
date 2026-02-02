command quad(...)
    local args = {...}
    local silent = false -- 新增：静默模式，-x 开启
    local showlog = false
    local showcomplex = false
    local a,b,c,varname = nil,nil,nil,nil

    for i,arg in ipairs(args) do
        if arg == "-x" then silent = true -- 静默标志，优先解析
        elseif arg == "-l" or arg == "-L" then showlog = true
        elseif arg == "-i" or arg == "-I" then showcomplex = true
        elseif not a and tonumber(arg) then a = tonumber(arg)
        elseif not b and tonumber(arg) then b = tonumber(arg)
        elseif not c and tonumber(arg) then c = tonumber(arg)
        elseif not varname then varname = arg end
    end


    if not a or not b or not c then
        if not silent then 
            print("参数错误 格式 quad [-x] [-l] [-i] a b c [变量名] 示例 quad 1 -5 6 res")
        end
        return
    end

    local log = ""
    log = log .. string.format("%.2fx^2 + %.2fx + %.2f = 0\n",a,b,c)
    local result = {}
    local res

    if a == 0 then
        log = log .. "a等于0 不是一元二次方程 转为一元一次方程求解\n"
        if b == 0 then
            result.type = c == 0 and "infinite" or "nosolution"
            res = c == 0 and "方程恒成立 x为任意实数" or "方程无解"
        else
            local x = -c/b
            log = log .. string.format("%.2fx + %.2f = 0\n",b,c)
            log = log .. string.format("x = -%.2f / %.2f\n",c,b)
            result.type = "linear"
            result.root = x
            res = string.format("非二次方程 一次解 x=%.4f",x)
        end
    else
        log = log .. "Δ = b^2 - 4ac\n"
        log = log .. string.format("Δ = %.2f^2 - 4*%.2f*%.2f\n",b,a,c)
        local delta = b^2 - 4*a*c
        log = log .. string.format("Δ = %.2f\n",delta)
        local den = 2*a

        if delta >= 0 then
            log = log .. "Δ大于等于0 方程有实数根\n"
            local sqrtd = math.sqrt(delta)
            log = log .. string.format("sqrtΔ = %.4f\n",sqrtd)
            log = log .. "x = (-b ± sqrtΔ) / (2a)\n"
            local x1 = (-b + sqrtd)/den
            local x2 = (-b - sqrtd)/den
            log = log .. string.format("x1 = (-%.2f + %.4f) / %.2f\n",b,sqrtd,den)
            log = log .. string.format("x2 = (-%.2f - %.4f) / %.2f\n",b,sqrtd,den)
            result.type = "real"
            result.delta = delta
            if x1 == x2 then
                log = log .. string.format("x1等于x2 = %.4f 重根\n",x1)
                result.root = x1
                res = string.format("唯一实数根 重根 x=%.4f",x1)
            else
                log = log .. string.format("x1 = %.4f\n",x1)
                log = log .. string.format("x2 = %.4f\n",x2)
                result.roots = {x1,x2}
                res = string.format("两个不同实数根 x1=%.4f x2=%.4f",x1,x2)
            end
        else
            log = log .. "Δ小于0 方程有共轭虚数根\n"
            log = log .. string.format("负Δ = %.2f\n",-delta)
            local sqrtd = math.sqrt(-delta)
            log = log .. string.format("sqrt负Δ = sqrt(%.2f) = %.4f\n",-delta,sqrtd)
            local real = -b/den
            local imag = sqrtd/den
            log = log .. string.format("实部 = -b / (2a) = -%.2f / %.2f = %.4f\n",b,den,real)
            log = log .. string.format("虚部 = sqrt负Δ / (2a) = %.4f / %.2f = %.4f\n",sqrtd,den,imag)
            result.type = "complex"
            result.delta = delta
            result.real = real
            result.imag = imag
            if showcomplex then
                log = log .. string.format("共轭虚数根 x1=%.4f+%.4fi x2=%.4f-%.4fi\n",real,imag,real,imag)
                res = string.format("共轭虚数根 x1=%.4f+%.4fi x2=%.4f-%.4fi",real,imag,real,imag)
            else
                log = log .. "方程有虚数根 加i查看详情\n"
                res = "方程有虚数根 加i查看详情"
            end
        end
    end


    if varname then 
        _G[varname] = result 
    end


    if not silent then
        print(string.format("方程 %.2fx^2 + %.2fx + %.2f = 0",a,b,c))
        print(res)
        if showlog then 
            print("\n" .. log) 
        end
    end
end


quad  1 -5 6 

print("你好")

