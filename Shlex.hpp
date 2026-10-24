#ifndef SHLEX_HPP_
#define SHLEX_HPP_

#include <cassert>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

// # The Shlex
//
// 现代 C++ 版本：利用 std::string, std::string_view 和 std::optional 重写。
// 兼具 POSIX Shell 语法的词法分析器(Lexer)与字符串拼接器(String Builder)功能。
// 内部自动管理内存，无需手动调用 free()。
//
// ## Splitting
//
// ```cpp
// Shlex s;
// s.init(source);
// while (auto token = s.next()) {
//     std::cout << *token << "\n";
// }
// ```
//
// ## Joining
//
// ```cpp
// Shlex s;
// s.append_quoted("foo");
// s.append_quoted("bar");
// s.append_quoted("Hello, World");
// std::cout << s.join() << "\n";
// ```

class Shlex {
private:
    constexpr static char TOKEN_DELIMITER = ' ';
    constexpr static std::string_view EMPTY_CMD_STRING = "''";

    // 1. 显式定义有限状态机的各个状态
    enum class LexState {
        Normal = 0, // 普通（无引号）模式
        SingleQuote, // 单引号模式
        DoubleQuote // 双引号模式
    };

    std::string_view source_;
    size_t pos_ { 0 };

    // 复用的内部动态字符串容器，取代手动 realloc 管理的 char*
    std::string buffer_;

    // ==========================================
    // 状态处理器 (State Handlers)
    // ==========================================

    auto peekChar()
        -> char
    {
        return source_[pos_];
    }

    auto getChar()
        -> char
    {
        return source_[pos_++];
    }

    auto noMoreChar()
        -> bool
    {
        return pos_ >= source_.size();
    }

    void skipTokenDelimter()
    {
        while (!noMoreChar() && (isTokenDelimiter(source_[pos_]))) {
            pos_++;
        }
    }

    void backPos()
    {
        pos_--;
    }

    static auto isTokenDelimiter(char c)
        -> bool
    {
        return std::isspace(c) != 0;
    }

    // 处理普通状态：返回 true 表示当前 Token 解析完成
    auto processNormalState(LexState& state)
        -> bool
    {
        auto c = getChar();

        if (c == '\'') {
            state = LexState::SingleQuote; // 状态转移 -> 单引号
        } else if (c == '"') {
            state = LexState::DoubleQuote; // 状态转移 -> 双引号
        } else if (c == '\\') {
            if (!noMoreChar()) {
                buffer_.push_back(getChar()); // 转义下一个字符
            }
        } else if (isTokenDelimiter(c)) {
            backPos(); // 回退一个字符，留给下次 next() 处理
            return true; // 遇到空白符，Token 分割结束
        } else {
            buffer_.push_back(c);
        }

        return false; // 继续解析
    }

    // 处理单引号状态
    void processSingleQuoteState(LexState& state)
    {
        auto c = getChar();

        if (c == '\'') {
            state = LexState::Normal; // 状态转移 -> 回到普通模式
        } else {
            buffer_.push_back(c); // 单引号内无转义，全盘接收
        }
    }

    // 处理双引号状态
    void processDoubleQuoteState(LexState& state)
    {
        char c = getChar();

        if (c == '"') {
            state = LexState::Normal; // 状态转移 -> 回到普通模式
        } else if (c == '\\') {
            if (noMoreChar()) {
                buffer_.push_back('\\');
                return;
            }

            auto nextChar = getChar();
            // POSIX 双引号内的特殊转义规则
            if (nextChar == '$' || nextChar == '`' || nextChar == '\\' || nextChar == '\n' || nextChar == '"') {
                buffer_.push_back(nextChar);
            } else {
                buffer_.push_back('\\');
                buffer_.push_back(nextChar);
            }
        } else {
            buffer_.push_back(c);
        }
    }

public:
    Shlex() = default;
    ~Shlex() = default;

    // 禁用拷贝以防共享内部缓冲区的意外情况，但允许移动
    Shlex(const Shlex&) = delete;
    Shlex& operator=(const Shlex&) = delete;
    Shlex(Shlex&&) noexcept = default;
    Shlex& operator=(Shlex&&) noexcept = default;

    // 设置要解析的源文本。默认会重置当前解析游标。
    void init(std::string_view source) noexcept
    {
        source_ = source;
        pos_ = 0;
        // 注意：不调用 buffer_.clear()，以防用户在 init 前调用了 append_quoted 且需要保留。
        // 但根据原本语义，init 代表开始一次 splitting，通常后续只调 next()，
        // 而 next() 内部会 clear()。
    }

    // 重置解析状态，清空输入源。保留 buffer_ 容量以备后续使用，不释放底层内存。
    void reset() noexcept
    {
        source_ = { };
        pos_ = 0;
        buffer_.clear();
    }

    // 切割并返回下一个 Token。
    // 返回 std::nullopt 表示已经解析到源文本末尾（没有更多 token）。
    // 优化后的 next() 函数：主干极其清晰
    auto next()
        -> std::optional<std::string>
    {
        if (noMoreChar()) {
            return std::nullopt; // 到达末尾
        }
        skipTokenDelimter();

        // 跳过前导空白后若已到末尾，说明源文本只剩空白，没有更多 token
        if (noMoreChar()) {
            return std::nullopt;
        }

        buffer_.clear();

        auto currentState = LexState::Normal;

        // 状态机主循环
        while (!noMoreChar()) {
            auto noMoreToken = false;

            // 根据当前状态，路由到对应的状态处理函数
            switch (currentState) {
            case LexState::Normal:
                noMoreToken = processNormalState(currentState);
                break;
            case LexState::SingleQuote:
                processSingleQuoteState(currentState);
                break;
            case LexState::DoubleQuote:
                processDoubleQuoteState(currentState);
                break;
            }

            if (noMoreToken) {
                return buffer_;
            }
        }

        // EOF 校验：如果字符串解析结束，但状态机没有回到 Normal，说明引号未闭合
        if (currentState == LexState::SingleQuote) {
            throw std::runtime_error("Parse Error: Unclosed single quote (') at EOF");
        } else if (currentState == LexState::DoubleQuote) {
            throw std::runtime_error("Parse Error: Unclosed double quote (\") at EOF");
        }

        return buffer_;
    }

    // 追加任意字符串，自动对 Shell 敏感字符加单引号转义
    void appendQuoted(std::string_view str)
    {
        if (!buffer_.empty()) {
            buffer_.push_back(TOKEN_DELIMITER);
        }

        if (str.empty()) {
            buffer_ += EMPTY_CMD_STRING;
            return;
        }

        if (!containsUnsafe(str)) {
            buffer_ += str;
            return;
        }

        buffer_.push_back('\'');
        for (char c : str) {
            if (c == '\'') {
                // POSIX Shell 无法在单引号内转义单引号，必须先闭合单引号，输出双引号包裹的单引号，再重新开启单引号
                buffer_ += "'\"'\"'";
            } else {
                buffer_.push_back(c);
            }
        }
        buffer_.push_back('\'');
    }

    // 返回拼接好的字符串，并清空内部状态以便安全进行下一次组合
    auto join()
        -> std::string
    {
        std::string result = std::move(buffer_);
        buffer_.clear();
        return result;
    }

private:
    static auto containsUnsafe(std::string_view str)
        -> bool
    {
        constexpr const std::string_view safeChars = "_@%+=:,./-";
        for (char c : str) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && safeChars.find(c) == std::string_view::npos) {
                return true;
            }
        }
        return false;
    }
};

#endif // SHLEX_HPP_
