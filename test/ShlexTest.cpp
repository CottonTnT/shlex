// Shlex 的 Catch2 测试套件
//
// 覆盖 Splitting（切割）、Joining（拼接）以及二者的互逆性（round-trip）。
// 使用 Catch2 v3，测试入口 main() 由 libCatch2Main.a 提供。

#include <catch2/catch_test_macros.hpp>

#include "Shlex.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

// 收集 source 切割出的全部 token
std::vector<std::string> splitAll(std::string_view source)
{
    Shlex s;
    s.init(source);
    std::vector<std::string> tokens;
    while (auto token = s.next()) {
        tokens.push_back(std::move(*token));
    }
    return tokens;
}

} // namespace

TEST_CASE("Shlex::next 按空白切分", "[shlex][splitting]")
{
    CHECK(splitAll("Foo Bar") == std::vector<std::string>{"Foo", "Bar"});
    CHECK(splitAll("-foo -bar -baz") == std::vector<std::string>{"-foo", "-bar", "-baz"});
    CHECK(splitAll("  a   b  ") == std::vector<std::string>{"a", "b"});
    CHECK(splitAll("\tTab\tSep\n") == std::vector<std::string>{"Tab", "Sep"});
    CHECK(splitAll("single") == std::vector<std::string>{"single"});
}

TEST_CASE("Shlex::next 处理反斜杠转义", "[shlex][splitting]")
{
    CHECK(splitAll(R"(Foo\ Bar)") == std::vector<std::string>{"Foo Bar"});
    CHECK(splitAll(R"(Foo\ \ Bar)") == std::vector<std::string>{"Foo  Bar"});
    CHECK(splitAll(R"(a\\b)") == std::vector<std::string>{R"(a\b)"});
}

TEST_CASE("Shlex::next 处理单引号", "[shlex][splitting]")
{
    CHECK(splitAll(R"('-foo -bar -baz')") == std::vector<std::string>{"-foo -bar -baz"});
    CHECK(splitAll(R"('a' 'b')") == std::vector<std::string>{"a", "b"});
    // 单引号内没有转义，反斜杠原样保留
    CHECK(splitAll(R"('a\b')") == std::vector<std::string>{R"(a\b)"});
}

TEST_CASE("Shlex::next 处理双引号", "[shlex][splitting]")
{
    CHECK(splitAll(R"("Hello, World")") == std::vector<std::string>{"Hello, World"});
    CHECK(splitAll(R"("a b"  "c d")") == std::vector<std::string>{"a b", "c d"});
}

TEST_CASE("Shlex::next 遵循 POSIX 双引号转义", "[shlex][splitting]")
{
    // 仅 $ ` \ " 换行 可被转义，其余反斜杠原样保留
    CHECK(splitAll(R"("a\$b")") == std::vector<std::string>{"a$b"});
    CHECK(splitAll(R"("a\\b")") == std::vector<std::string>{R"(a\b)"});
    CHECK(splitAll(R"("a\"b")") == std::vector<std::string>{R"(a"b)"});
    CHECK(splitAll(R"("a\nb")") == std::vector<std::string>{R"(a\nb)"});
}

TEST_CASE("Shlex::next 处理混合引号", "[shlex][splitting]")
{
    auto tokens = splitAll(R"(-I"./raylib/" -C link-args="-L\"./hello world\" -lm -lc" -O3)");
    CHECK(tokens == std::vector<std::string>{
        "-I./raylib/", "-C", R"(link-args=-L"./hello world" -lm -lc)", "-O3"});
}

TEST_CASE("空输入或纯空白输入没有 token", "[shlex][splitting]")
{
    CHECK(splitAll("").empty());
    CHECK(splitAll("   \t  ").empty());
}

TEST_CASE("Shlex::next 在引号未闭合时抛出异常", "[shlex][splitting]")
{
    Shlex s;
    s.init("'unclosed");
    CHECK_THROWS_AS(s.next(), std::runtime_error);

    s.init("\"unclosed");
    CHECK_THROWS_AS(s.next(), std::runtime_error);
}

TEST_CASE("Shlex::appendQuoted 拼接安全字符串不加引号", "[shlex][joining]")
{
    Shlex s;
    s.appendQuoted("foo");
    s.appendQuoted("bar");
    s.appendQuoted("baz");
    CHECK(s.join() == "foo bar baz");
}

TEST_CASE("Shlex::appendQuoted 为不安全字符加单引号", "[shlex][joining]")
{
    Shlex s;
    s.appendQuoted("foo");
    s.appendQuoted("bar baz");
    CHECK(s.join() == "foo 'bar baz'");

    Shlex t;
    t.appendQuoted("a'b");
    CHECK(t.join() == R"('a'"'"'b')");
}

TEST_CASE("Shlex::appendQuoted 处理空串", "[shlex][joining]")
{
    Shlex s;
    s.appendQuoted("");
    CHECK(s.join() == "''");

    Shlex t;
    t.appendQuoted("");
    t.appendQuoted("x");
    CHECK(t.join() == "'' x");
}

TEST_CASE("拼接结果可被切割还原（round-trip）", "[shlex][roundtrip]")
{
    Shlex s;
    s.appendQuoted("foo");
    s.appendQuoted("bar");
    s.appendQuoted("baz");
    s.appendQuoted("Hello, 'World'");
    std::string joined = s.join();

    CHECK(splitAll(joined) == std::vector<std::string>{"foo", "bar", "baz", "Hello, 'World'"});
}

TEST_CASE("Shlex::init 重置解析游标", "[shlex]")
{
    Shlex s;
    s.init("foo bar");
    CHECK(s.next() == std::string("foo"));
    CHECK(s.next() == std::string("bar"));
    CHECK_FALSE(s.next().has_value());

    // 重新 init 后从新源文本的开头解析
    s.init("baz qux");
    CHECK(s.next() == std::string("baz"));
}

TEST_CASE("Shlex::reset 清空输入源", "[shlex]")
{
    Shlex s;
    s.init("foo");
    CHECK(s.next() == std::string("foo"));
    s.reset();
    CHECK_FALSE(s.next().has_value());
}
