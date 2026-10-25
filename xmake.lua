-- shlex 模块构建脚本
--
-- Shlex 是头文件单文件实现的 POSIX Shell 词法分析器 / 拼接器。
-- 作为一个独立模块，这里同时定义库目标与测试目标：
--   1. shlex      —— 头文件库（headeronly），供其他模块 include 使用
--   2. shlex_test —— Catch2 测试（test/ 目录），通过 `xmake test` 运行

-- Catch2 测试框架依赖（xmake 包管理，自动下载/构建并链接）
add_requires("catch2")
add_rules("plugin.compile_commands.autoupdate", {outputdir=".vscode"})

-- 头文件库：下游目标 add_deps("shlex") 后即可 #include "Shlex.hpp"
target("shlex")
    set_kind("headeronly")
    add_headerfiles("Shlex.hpp")
    add_includedirs(".", {public = true})
    set_languages("c++17")
target_end()

-- Catch2 测试：components 选择 Catch2 v3 的 main（提供测试入口）与 lib（测试框架）
target("shlex_test")
    set_kind("binary")
    add_deps("shlex")
    add_files("test/*.cpp")
    set_languages("c++17")
    add_cxflags("-Wall", "-Wextra")
    add_packages("catch2", {components = {"main", "lib"}})
    add_tests("shlex_test")
target_end()
