#include <array>
#include <esc/esc.hpp>
#include <string>
#include <string_view>

int main()
{
    using namespace esc;
    constexpr std::array<std::string_view, 3> names = {"Café", "東京", "e\u0301clair"};
    auto heading = fg(colors::cyan) | bold;
    auto profile = resolve_profile(snapshot_environment(), true).profile;
    std::string table = encode(profile, styled("Name      Status", heading), "\n");
    for (auto name : names) {
        auto width = cell_width(name).value();
        append_encoded(table, profile, name, std::string(10 - width, ' '),
                       styled("ready", fg(colors::green)), "\n");
    }
    Output{}.write(table);
    flush();
}
