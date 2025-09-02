add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
target("RyanMqtt",function()
    set_kind("binary")

    add_syslinks("pthread")
    set_toolchains("gcc")  -- 确保使用 GCC
    -- set_toolchains("clang-20")  
    set_languages("gnu99") -- 启用 GNU 扩展

    add_defines("PKG_USING_RYANMQTT_IS_ENABLE_ASSERT") -- 开启assert
    add_defines("RyanMqttLinuxTestEnable") -- linux测试

    -- set_optimize("smallest") -- -Os
    -- set_optimize("faster") -- -O2
    -- set_optimize("fastest") -- -O3
    set_optimize("aggressive") -- -Ofast

    -- 启用全部警告
    set_warnings("everything") -- -Wall -Wextra -Weffc++ / -Weverything

    -- 链接器选项：生成 map 文件
    add_ldflags("-Wl,-Map=$(buildir)/RyanMqtt.map")

    -- Sanitizer 检测项（运行时错误）
    add_ldflags(
        "-fsanitize=address", -- 内存越界、释放后使用
        "-fsanitize=leak", -- 内存泄漏
        "-fsanitize=undefined", -- 未定义行为（除零、溢出、无效移位等）
        "-fsanitize=pointer-compare", -- 无效指针比较
        "-fsanitize=pointer-subtract", -- 无效指针相减
        "-fsanitize=bounds", -- 数组越界
        "-fsanitize=float-divide-by-zero", -- 浮点除零
        "-fsanitize=float-cast-overflow", -- 浮点转整数溢出
        -- "-fsanitize=alignment", -- 检测未对齐的内存访问
        "-fno-sanitize=alignment", -- 某些平台不兼容
        {force = true}
    )

    -- 编译器警告与静态分析（开发期错误检测）
    add_cxflags(
        "-flto", -- 链接时优化（可选）
        "-pedantic", -- 强制遵循 ISO C/C++ 标准
        "-Wall", -- 启用大多数常见警告
        "-Wextra", -- 启用额外警告
        "-fanalyzer", -- 启用 gcc 静态分析器
        "-Wno-unused-parameter",
        "-Wfloat-equal", -- 浮点直接比较
        "-Wshadow", -- 局部变量遮蔽
        "-Wcast-align", -- 类型转换对齐问题
        "-Wpointer-arith", -- 指针运算
        "-Warray-bounds=2", -- 数组越界访问（编译期可分析到的）
        "-Wstringop-overflow=2", -- memcpy / strcpy 等可能的溢出
        "-Wstringop-truncation", -- 字符串截断风险
        "-Walloc-zero", -- malloc(0) 等分配 0 字节的情况
        "-Wfree-nonheap-object", -- 释放非堆对象
        -- "-Wconversion", -- 隐式类型转换可能导致精度丢失/溢出
        "-Wnull-dereference", -- 空指针解引用
        "-Wlogical-op", -- 逻辑运算符误用
        "-Wstrict-overflow=5", -- 有符号溢出优化假设
        "-Wmissing-prototypes", -- 未在头文件声明的全局函数
        "-Wmissing-declarations", -- 未声明的全局变量/函数
        "-Wredundant-decls", -- 重复声明
        "-Wunreachable-code", -- 不可达代码
        "-Wunsafe-loop-optimizations", -- 循环优化可能引入的问题
        "-Wtype-limits", -- 比较恒真/恒假的表达式（如 unsigned < 0）
        "-Wshift-overflow=2", -- 移位导致的溢出
        "-Wshift-negative-value", -- 对负数进行移位
        "-Wdiv-by-zero", -- 除以零（编译期可分析到的）
        "-Wformat-security", -- 格式化字符串安全问题
        "-Wdisabled-optimization", -- 被禁用的优化
        -- "-Wwrite-strings", -- 将字符串字面量视为 const char[]，防止被修改
        "-Wreturn-local-addr", -- 返回局部变量地址
        "-Wuse-after-free", -- 释放后使用（编译期可分析到的）
        "-Wdangling-pointer=2", -- 悬空指针（GCC 12+）
        "-fstack-protector-strong", -- 栈保护

        -- clang 的兼容项（可选）
        -- "-Wno-gnu-zero-variadic-macro-arguments",
        -- "-Wno-c23-extensions",
        {force = true}
    )

    
    --加入代码和头文件
    add_includedirs('./common', {public = true})
    add_includedirs('./coreMqtt', {public = true})
    add_includedirs('./mqttclient/include', {public = true})
    add_includedirs('./platform/linux', {public = true})
    add_includedirs('./platform/linux/valloc', {public = true})

    add_files('./test/*.c', {public = true})
    add_files('./common/*.c', {public = true})
    add_files('./coreMqtt/*.c', {public = true})
    add_files('./mqttclient/*.c', {public = true})
    add_files('./platform/linux/*.c', {public = true})
    add_files('./platform/linux/valloc/*.c', {public = true})

end)
