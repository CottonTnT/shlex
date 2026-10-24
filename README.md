# Shlex

现代 C++ 实现的 POSIX Shell 风格词法分析器（Lexer）与字符串拼接器（Joiner）。

它相当于 Python 标准库 [`shlex`](https://docs.python.org/3/library/shlex.html) 中
`split` / `join` 的 C++ 版本：既能按照 Shell 的引用与转义规则把一条命令字符串切成若干个
**Token**，也能把若干参数按同样的规则**拼回**一条可直接交给 Shell 执行的命令字符串。

整个模块是**头文件单文件实现**（`Shlex.hpp`），无需编译、链接额外的库，`#include "Shlex.hpp"`
即可使用，内部由 `std::string` / `std::string_view` 自动管理内存，没有手动 `free()`。

## 特性

- 纯头文件、零依赖，只需 C++17 标准库。
- **Splitting（切割）**：按 POSIX Shell 规则把命令串切分为 Token，支持单引号、双引号、反斜杠转义。
- **Joining（拼接）**：把参数串重新拼接为可直接执行的安全命令串，自动为不安全字符加引号。
- 有限状态机（FSM）驱动的词法分析，状态清晰、可扩展。
- 显式、可预期的错误处理：引号未闭合时抛出异常。
- 复用内部缓冲区，减少反复分配。

## 环境要求

- C++17 及以上（使用了 `std::optional` 与 `std::string_view`）。
- 任意支持 C++17 的编译器（GCC / Clang / MSVC）。

## 快速开始

```cpp
#include "Shlex.hpp"
#include <iostream>

int main() {
    Shlex s;

    // Splitting：把命令串切成 token
    s.init("\"Hello, World\"     'Foo Bar'");
    while (auto token = s.next()) {
        std::cout << "[" << *token << "]\n";
    }
    // [Hello, World]
    // [Foo Bar]

    // Joining：把参数拼回可执行命令串
    s.appendQuoted("foo");
    s.appendQuoted("bar baz");
    s.appendQuoted("Hello, 'World'");
    std::cout << s.join() << "\n";
    // foo 'bar baz' 'Hello, '"'"'World'"'"''
}
```

## API 参考

`Shlex` 是一个可复用、**非线程安全**的类：一个实例同一时刻只能被一个线程使用。它不可拷贝
（拷贝构造 / 拷贝赋值被 `= delete`），但可移动。

| 成员 | 说明 |
| --- | --- |
| `void init(std::string_view source)` | 设置待解析的源文本并重置游标到开头。**不会**清空拼接缓冲区（见下文 `appendQuoted`）。 |
| `void reset()` | 清空输入源、游标与缓冲区，保留底层内存容量以便复用。 |
| `std::optional<std::string> next()` | 切割并返回下一个 Token；返回 `std::nullopt` 表示已到源文本末尾。引号未闭合时抛出 `std::runtime_error`。 |
| `void appendQuoted(std::string_view str)` | 把一段文本追加到拼接缓冲区，自动加引号转义。多个片段之间以单个空格分隔。 |
| `std::string join()` | 返回拼接结果并清空缓冲区，以便安全进行下一次拼接。 |

## Splitting（切割）规则详解

`next()` 基于一个三状态有限状态机逐字符扫描源文本：

```
Normal ──'──> SingleQuote ──'──> Normal
  │  └──"──> DoubleQuote ──"──┘
```

### 普通模式（`Normal`）

- `'`：进入单引号模式，**不**把引号本身写入结果。
- `"`：进入双引号模式，**不**把引号本身写入结果。
- `\`：转义下一个字符——若后面还有字符，则把下一个字符原样写入结果；
  若 `\` 已是最后一个字符，则该反斜杠被静默丢弃。
- 空白字符（`std::isspace`，含空格、制表符、换行等）：回退游标并结束当前 Token。
- 其他字符：直接写入结果。

### 单引号模式（`SingleQuote`）

- 单引号内**无任何转义**，所有字符（包括 `\`、`"`）原样接收。
- 遇到 `'` 关闭单引号并回到普通模式。

### 双引号模式（`DoubleQuote`）

- 遇到 `"` 关闭双引号并回到普通模式。
- `\` 遵循 **POSIX 双引号内的特殊转义规则**：仅当被转义字符是
  `$`、`` ` ``、`\`、`"` 或换行时，才去掉反斜杠只保留该字符；
  否则保留反斜杠本身及其后的字符（即 `\x` 原样保留为 `\x`）。
- 其余字符原样接收。

### 结束与错误处理

- 游标到达末尾即结束，返回当前缓冲区内容（若缓冲区为空则返回空串）。
- 若源文本结束时状态机仍停留在 `SingleQuote` 或 `DoubleQuote`，说明**引号未闭合**，
  `next()` 抛出 `std::runtime_error`，错误信息分别为：

  - `Parse Error: Unclosed single quote (') at EOF`
  - `Parse Error: Unclosed double quote (") at EOF`

### 切割示例

| 输入 | 输出 Token |
| --- | --- |
| `Foo Bar` | `Foo`, `Bar` |
| `Foo\ Bar` | `Foo Bar` |
| `-foo -bar -baz` | `-foo`, `-bar`, `-baz` |
| `'-foo -bar -baz'` | `-foo -bar -baz` |
| `"Hello, World"     'Foo Bar'` | `Hello, World`, `Foo Bar` |
| `-I"./raylib/" -C link-args="-L\"./hello world\" -lm -lc" -O3` | `-I./raylib/`, `-C`, `link-args=-L"./hello world" -lm -lc`, `-O3` |

## Joining（拼接）规则详解

`appendQuoted()` 把一段文本按 Shell 规则转义后追加到缓冲区，多段文本之间以**单个空格**
（`TOKEN_DELIMITER`）分隔：

1. 若缓冲区非空，先追加一个分隔空格。
2. 若文本为空，追加空串的字面量 `''`（`EMPTY_CMD_STRING`）。
3. 若文本只含**安全字符**，则原样追加（不加引号）。
4. 否则用单引号 `'...'` 包裹：因为 POSIX Shell 无法在单引号内部转义单引号本身，
   内嵌的单引号会被替换为 `'\"'\"'`（即「先闭合单引号 → 双引号包裹的单引号 → 重新开启单引号」）。

安全字符集合定义为：字母数字（`std::isalnum`）以及 `_ @ % + = : , . / -`。

### 拼接示例

| 调用 | 输出 |
| --- | --- |
| `foo` + `bar` + `baz` | `foo bar baz` |
| `foo` + `bar baz` | `foo 'bar baz'` |
| `foo` + `bar` + `baz` + `Hello, 'World'` | `foo bar baz 'Hello, '"'"'World'"'"''` |
| `a'b` | `'a'"'"'b'` |

> 切割与拼接是互逆的：`appendQuoted` 得到的字符串交给 `init` + `next`，能还原出原来的参数。

## 测试（Catch2）

测试已从 `Shlex.hpp` 中分离出来，位于 [test/](test/) 目录，使用
[Catch2](https://github.com/catchorg/Catch2)（v3）框架，覆盖切割、拼接、POSIX 转义规则、
异常处理以及「先拼再切」的回环测试。

Catch2 依赖已在 [xmake.lua](xmake.lua) 中通过 `add_requires("catch2")` 声明，由 xmake
自动解析（优先使用系统安装，否则自动下载构建），无需手动指定链接路径。构建与运行：

```bash
xmake build shlex_test   # 编译测试程序
xmake run shlex_test     # 直接运行测试二进制
xmake test               # 构建并运行所有已注册的测试
```

运行输出（示例）：

```
===============================================================================
All tests passed (34 assertions in 14 test cases)
```

## 已知问题与注意事项

- `Shlex` 的 `next()` / `appendQuoted()` 会共享内部缓冲区，**并非线程安全**，多线程请各自持有实例。
- 源文本最后一个字符若是普通模式下未被消费的反斜杠，会被静默丢弃。
