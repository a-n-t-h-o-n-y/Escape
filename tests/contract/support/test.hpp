#pragma once

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace contract {
struct Test {
    char const* name;

    void (*run)();
};

inline auto tests() -> std::vector<Test>&
{
    static std::vector<Test> result;
    return result;
}

struct Register {
    Register(char const* name, void (*run)()) { tests().push_back({name, run}); }
};

[[noreturn]] inline void fail(char const* file, int line, std::string_view message)
{
    std::ostringstream out;
    out << file << ':' << line << ": " << message;
    throw std::runtime_error(out.str());
}

template <class Exception, class Function>
void throws(Function&& function, char const* file, int line)
{
    try {
        function();
    }
    catch (Exception const&) {
        return;
    }
    catch (std::exception const& error) {
        fail(file, line, std::string{"wrong exception: "} + error.what());
    }
    catch (...) {
        fail(file, line, "wrong nonstandard exception");
    }
    fail(file, line, "expected exception was not thrown");
}
}  // namespace contract

#define TEST(name)                                           \
    static void name();                                      \
    static contract::Register register_##name{#name, &name}; \
    static void name()
#define CHECK(...)                                                            \
    do {                                                                      \
        if (!(__VA_ARGS__)) contract::fail(__FILE__, __LINE__, #__VA_ARGS__); \
    } while (false)
#define CHECK_THROWS(type, ...)                                                 \
    contract::throws<type>([&] { static_cast<void>((__VA_ARGS__)); }, __FILE__, \
                           __LINE__)
