#include <esc/esc.hpp>
#include <iostream>
#include <type_traits>

using namespace esc;

void display_event(Terminal& terminal, Event const& event)
{
    std::visit(
        [&](auto const& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::same_as<T, KeyPress>) {
                terminal.write("Key: ");
                if (auto scalar = text_scalar(value.key))
                    terminal.write("scalar ",
                                   std::to_string(static_cast<unsigned>(*scalar)));
                else
                    terminal.write("named ", std::to_string(static_cast<unsigned>(
                                                 std::get<KeyCode>(value.key))));
                terminal.write(" shift=", value.modifiers.shift ? "yes" : "no",
                               " ctrl=", value.modifiers.ctrl ? "yes" : "no",
                               " alt=", value.modifiers.alt ? "yes" : "no");
            }
            else if constexpr (std::same_as<T, Resize>) {
                terminal.write("Resize: ", std::to_string(value.size.width), " x ",
                               std::to_string(value.size.height));
            }
            else if constexpr (std::same_as<T, Focus>) {
                terminal.write(value.focused ? "Focused" : "Unfocused");
            }
            else if constexpr (std::same_as<T, PasteChunk>) {
                terminal.write("Paste bytes: ", std::to_string(value.bytes.size()),
                               value.last ? " (last)" : "",
                               value.aborted ? " (aborted)" : "");
            }
            else if constexpr (std::same_as<T, UnknownInput>) {
                // Report the count; never echo an unknown control unit.
                terminal.write("Unknown input bytes: ",
                               std::to_string(value.bytes.size()));
            }
            else {
                terminal.write("Mouse at ", std::to_string(value.at.x), ",",
                               std::to_string(value.at.y));
            }
        },
        event);
}
int main()
{
    try {
        auto terminal = Terminal::open(TerminalOptions::interactive());
        terminal.send(BlankScreen{}, Cursor{});
        terminal.write("Escape capability/input inspector — press q to quit\r\n");
        for (auto const& decision : terminal.profile_report().decisions)
            terminal.write(decision.explanation, "\r\n");
        terminal.write("System palette: ");
        for (unsigned i = 0; i < 16; ++i) {
            terminal.send(fg(XColor{static_cast<std::uint8_t>(i)}));
            terminal.write("██");
        }
        terminal.send(ResetStyle{});
        terminal.write("\r\n");
        // Opt into optional input features only when the selected profile supports
        // them.
        auto const& profile = terminal.profile();
        if (profile.mouse_protocol != MouseProtocol::None)
            terminal.set(profile.maximum_mouse_mode);
        if (profile.focus_reporting) terminal.set(FocusReporting::On);
        if (profile.bracketed_paste) terminal.set(BracketedPaste::On);
        terminal.flush();
        while (true) {
            auto result = terminal.read();
            if (result.status == ReadStatus::EndOfInput) break;
            if (auto key = std::get_if<KeyPress>(&*result.event);
                key && text_scalar(key->key) == std::optional<char32_t>{U'q'})
                break;
            terminal.send(Cursor{0, 13}, BlankRow{});
            display_event(terminal, *result.event);
            terminal.flush();
        }
        terminal.close();
        return 0;
    }
    catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
