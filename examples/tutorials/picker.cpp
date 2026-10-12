#include <array>
#include <esc/esc.hpp>
#include <string_view>

int main()
{
    using namespace esc;
    constexpr std::array<std::string_view, 3> choices = {"Local", "Staging",
                                                         "Production"};
    auto accent = fg(colors::cyan) | bold;
    auto terminal = Terminal::open(TerminalOptions::interactive());
    unsigned selected = 0;

    for (;;) {
        auto frame = terminal.frame();
        terminal.write(Cursor{}, styled("Choose a destination", bold),
                       BlankRow{Erase::ToEnd}, "\r\n");
        for (unsigned i = 0; i < choices.size(); ++i)
            terminal.write(styled(sequence(i == selected ? "> " : "  ", choices[i]),
                                  i == selected ? accent : StylePatch{}),
                           BlankRow{Erase::ToEnd}, "\r\n");
        terminal.write(styled("Up/Down choose · Enter accept · q cancel", dim),
                       BlankRow{Erase::ToEnd});
        frame.close();

        auto input = terminal.read();
        if (std::holds_alternative<EndOfInput>(input)) return 0;
        auto event = std::get_if<Event>(&input);
        auto key = event ? std::get_if<KeyEvent>(event) : nullptr;
        if (!key) continue;
        if (key->matches(U'q') || key->matches(KeyCode::Escape)) return 0;
        if (key->matches(KeyCode::ArrowDown))
            selected = (selected + 1) % choices.size();
        if (key->matches(KeyCode::ArrowUp))
            selected = (selected + choices.size() - 1) % choices.size();
        if (key->matches(KeyCode::Enter)) break;
    }
    terminal.close();
    write("Selected ", choices[selected], "\n");
    flush();
}
