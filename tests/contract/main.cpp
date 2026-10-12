#include "support/test.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    auto const filter = argc > 1 ? std::string_view{argv[1]} : std::string_view{};
    int failed = 0;
    int selected = 0;
    for (auto const& test : contract::tests()) {
        if (std::string_view{test.name}.find(filter) == std::string_view::npos)
            continue;
        ++selected;
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        }
        catch (std::exception const& error) {
            ++failed;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
        catch (...) {
            ++failed;
            std::cerr << "FAIL " << test.name << ": unknown exception\n";
        }
    }
    std::cout << selected << " tests selected, " << failed << " failed\n";
    return selected == 0 ? 2 : failed == 0 ? 0 : 1;
}
