#include <esc/esc.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#ifdef _WIN32
#    include <io.h>
#else
#    include <unistd.h>
#endif

using namespace esc;
using namespace std::chrono_literals;
namespace {
constexpr std::array<std::string_view, 10> pages = {
    "System palette", "256 colors",     "True color",      "Text styles",
    "Unicode",        "Input events",   "Cursor controls", "Hyperlinks",
    "Sound",          "Probe & results"};
constexpr std::array cursor_styles = {
    CursorStyle::BlinkingBlock,   CursorStyle::BlinkingUnderline,
    CursorStyle::BlinkingBar,     CursorStyle::SteadyBlock,
    CursorStyle::SteadyUnderline, CursorStyle::SteadyBar};
constexpr std::array<std::string_view, 6> cursor_names = {
    "Blinking block", "Blinking underline", "Blinking bar",
    "Steady block",   "Steady underline",   "Steady bar"};
constexpr std::array<std::string_view, 17> features = {"Control sequences",
                                                       "Colors",
                                                       "Text styles",
                                                       "Alternate screen",
                                                       "Cursor visibility",
                                                       "Mouse",
                                                       "Focus",
                                                       "Bracketed paste",
                                                       "Cursor shape",
                                                       "Cursor save/restore",
                                                       "Scrolling",
                                                       "Editing",
                                                       "Window title",
                                                       "Hyperlinks",
                                                       "Synchronized output",
                                                       "Bell",
                                                       "DECPS notes"};
auto safe(std::string_view text) -> std::string
{
    std::string result;
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : text) {
        if (c >= 32 && c < 127)
            result += static_cast<char>(c);
        else {
            result += "\\x";
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    return result;
}
auto fit(std::string_view text, int columns) -> std::string
{
    std::string result;
    while (!text.empty()) {
        auto cluster = next_grapheme(text);
        auto width = cell_width(cluster).value_or(1);
        if (columns <= 0 || width > static_cast<std::size_t>(columns)) break;
        result += cluster;
        columns -= static_cast<int>(width);
        text.remove_prefix(cluster.size());
    }
    return result;
}
auto reply_text(Reply const& reply) -> std::string
{
    return match(
        reply,
        [](DeviceAttributesReply const& r) {
            std::string s = "Device attributes:";
            for (auto n : r.attributes)
                s += " " + std::to_string(n);
            return s;
        },
        [](KeyboardReply r) { return "Kitty flags: " + std::to_string(r.flags); },
        [](ModeReply r) {
            constexpr std::string_view states[] = {
                "unrecognized", "set", "reset", "permanently set", "permanently reset"};
            auto i = static_cast<unsigned>(r.status);
            return "Mode " + std::to_string(r.mode) + ": " +
                   std::string{i < 5 ? states[i] : "invalid reply"};
        });
}
auto outcome(QueryResult const& r) -> std::string
{
    return match(
        r, [](Reply const& v) { return reply_text(v); },
        [](Timeout) { return std::string{"No reply (timeout); support is unknown"}; },
        [](EndOfInput) { return std::string{"Input closed"}; },
        [](InputPending) {
            return std::string{"Input pending; retry after draining events"};
        },
        [](Rejected const& v) { return "Rejected: " + reply_text(v.reply); });
}
auto key_name(Key const& key) -> std::string
{
    return match(
        key,
        [](char32_t c) {
            std::ostringstream s;
            s << "U+" << std::uppercase << std::hex << std::setfill('0') << std::setw(4)
              << static_cast<unsigned>(c);
            if (c >= 32 && c < 127) s << " '" << static_cast<char>(c) << "'";
            return s.str();
        },
        [](KeyCode c) {
            constexpr std::string_view names[] = {
                "Escape", "Enter", "Tab", "Backspace", "Up",     "Down",   "Left",
                "Right",  "Home",  "End", "Insert",    "Delete", "PageUp", "PageDown"};
            auto i = static_cast<unsigned>(c);
            if (i < std::size(names)) return std::string{names[i]};
            if (c >= KeyCode::F1 && c <= KeyCode::F24)
                return "F" + std::to_string(i - static_cast<unsigned>(KeyCode::F1) + 1);
            return "KeyCode " + std::to_string(i);
        },
        [](KittyKey k) { return "Kitty key " + std::to_string(k.code); });
}
auto event_text(Event const& event) -> std::string
{
    return std::visit(
        [](auto const& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::same_as<T, KeyEvent>) {
                std::string s = v.key ? key_name(*v.key) : "No key identity";
                s += v.action == KeyAction::Release  ? " release"
                     : v.action == KeyAction::Repeat ? " repeat"
                                                     : " press";
                if (v.modifiers.ctrl) s += " Ctrl";
                if (v.modifiers.alt) s += " Alt";
                if (v.modifiers.shift) s += " Shift";
                if (v.modifiers.super) s += " Super";
                if (v.modifiers.meta) s += " Meta";
                if (v.modifiers.hyper) s += " Hyper";
                if (v.modifiers.caps_lock) s += " CapsLock";
                if (v.modifiers.num_lock) s += " NumLock";
                if (v.shifted_key) s += " shifted=" + key_name(Key{*v.shifted_key});
                if (v.base_layout_key)
                    s += " base=" + key_name(Key{*v.base_layout_key});
                if (!v.text.empty()) s += " text=" + safe(utf32_to_utf8(v.text));
                return s;
            }
            else if constexpr (std::same_as<T, Resize>)
                return "Resize " + std::to_string(v.size.width) + " x " +
                       std::to_string(v.size.height);
            else if constexpr (std::same_as<T, Focus>)
                return v.focused ? "Focus gained" : "Focus lost";
            else if constexpr (std::same_as<T, PasteChunk>)
                return "Paste " + std::to_string(v.bytes.size()) + " bytes" +
                       (v.first ? " start" : "") + (v.last ? " end" : "") +
                       (v.aborted ? " aborted" : "") + ": " +
                       safe(v.bytes.substr(0, 80));
            else if constexpr (std::same_as<T, UnknownInput>)
                return "Unknown input: " + safe(v.bytes.substr(0, 40));
            else if constexpr (std::same_as<T, TerminalReply>)
                return reply_text(v.value);
            else {
                std::string s;
                if constexpr (std::same_as<T, MousePress>)
                    s = "Mouse press button " +
                        std::to_string(static_cast<unsigned>(v.button) + 1);
                else if constexpr (std::same_as<T, MouseRelease>)
                    s = "Mouse release";
                else if constexpr (std::same_as<T, MouseMove>)
                    s = "Mouse move";
                else
                    s = "Wheel " + std::to_string(v.dx) + "," + std::to_string(v.dy);
                return s + " at " + std::to_string(v.at.x) + "," +
                       std::to_string(v.at.y);
            }
        },
        event);
}
auto tty(std::FILE* stream) -> bool
{
#ifdef _WIN32
    return _isatty(_fileno(stream)) != 0;
#else
    return isatty(fileno(stream)) != 0;
#endif
}
void report(Environment const& env, bool output_tty)
{
    auto r = resolve_profile(env, output_tty);
    std::cout << "Escape / termcaps - capability assumptions\nTERM: " << safe(env.term)
              << "\nCOLORTERM: " << safe(env.colorterm)
              << "\nNO_COLOR: " << (env.no_color ? "set" : "unset") << '\n';
    for (auto const& d : r.decisions)
        std::cout << features[static_cast<unsigned>(d.feature)] << ": "
                  << safe(d.explanation) << '\n';
    std::cout << "\nThese are assumptions, not test results. Run in a terminal for "
                 "interactive tests.\n";
}
class Tester {
    Terminal& terminal;
    Environment env;
    ProfileReport assumptions;
    Area size{};
    std::string frame;
    std::deque<std::string> events;
    std::vector<std::string> probes;
    unsigned page = 0;
    bool chrome_color = true, mouse = false, focus = false, paste = false,
         cursor_visible = false;
    bool show_assumptions = false;
    char control_sample = 0;
    int cursor_y = 12;
    unsigned palette_offset = 0, report_offset = 0, cursor_shape = 0;
    std::string message = "Explore samples; press p to query terminal capabilities.";
    int x = 24, y = 7, width = 50;
    void row(int column,
             int line,
             std::string_view text,
             Traits traits = {},
             bool accent = false)
    {
        if (line < 0 || line >= size.height - 1 || column >= size.width - 1) return;
        StylePatch appearance{.enabled = traits};
        if (accent && chrome_color) appearance.foreground = colors::cyan;
        append_encoded(frame, Cursor{column, line},
                       styled(fit(text, size.width - column - 1), appearance));
    }
    void line(std::string_view text, Traits traits = {})
    { row(x, y++, fit(text, width), traits); }
    void heading(std::string_view text)
    {
        row(x, y++, fit(text, width), bold, true);
        ++y;
    }
    void sample(std::string_view text, StylePatch patch)
    {
        if (y >= size.height - 5) return;
        append_encoded(frame, Cursor{x, y++}, styled(fit(text, width), patch));
    }
    void body()
    {
        switch (page) {
            case 0:
                heading("16 SYSTEM COLORS");
                line("Colors follow your terminal's theme.");
                ++y;
                for (unsigned group = 0; group < 2; ++group) {
                    for (unsigned i = 0; i < 8; ++i) {
                        auto index = group * 8 + i;
                        append_encoded(
                            frame, Cursor{x + static_cast<int>(i) * 5, y},
                            styled("    ",
                                   bg(IndexedColor{static_cast<std::uint8_t>(index)})));
                        row(x + static_cast<int>(i) * 5, y + 1, std::to_string(index));
                    }
                    y += 3;
                }
                line("Distinct regular and bright swatches?");
                break;
            case 1:
                heading("256-COLOR PALETTE");
                line("Up / Down scroll; row labels are slot indices.");
                ++y;
                for (unsigned r = palette_offset; r < 16 && y < size.height - 5;
                     ++r, ++y) {
                    row(x, y, std::to_string(r * 16));
                    for (unsigned c = 0; c < 16; ++c)
                        append_encoded(
                            frame, Cursor{x + 4 + static_cast<int>(c) * 2, y},
                            styled("  ", bg(IndexedColor{
                                             static_cast<std::uint8_t>(r * 16 + c)})));
                }
                break;
            case 2:
                heading("RGB / TRUE COLOR");
                line("Smooth gradients suggest true-color rendering.");
                ++y;
                for (unsigned band = 0; band < 3; ++band) {
                    for (int i = 0; i < width; ++i) {
                        auto t = static_cast<std::uint8_t>(255 * i / (width - 1));
                        auto color =
                            band == 0   ? RGB{t, 0, static_cast<std::uint8_t>(255 - t)}
                            : band == 1 ? RGB{0, t, static_cast<std::uint8_t>(255 - t)}
                                        : RGB{t, t, t};
                        append_encoded(frame, Cursor{x + i, y}, styled(" ", bg(color)));
                    }
                    y += 2;
                }
                line("Look for smooth changes rather than steps.");
                break;
            case 3:
                heading("TEXT ATTRIBUTES");
                sample("Bold          heavier / brighter", bold | StylePatch{});
                sample("Dim           lower intensity", dim | StylePatch{});
                sample("Italic        slanted glyphs", italic | StylePatch{});
                sample("Underline     single line", underline | StylePatch{});
                sample("Double        double underline",
                       double_underline | StylePatch{});
                sample("Crossed out   line through text", crossed_out | StylePatch{});
                sample("Inverse       swapped colors", inverse | StylePatch{});
                sample("Blink         may be disabled by preference",
                       blink | StylePatch{});
                append_encoded(frame, Cursor{x, y++}, "Hidden: [",
                               styled("secret", invisible), "] should be empty");
                line("Plain text should be normal again.");
                break;
            case 4:
                heading("UNICODE / CELL ALIGNMENT");
                line("Each closing | should land on the same column.");
                ++y;
                for (std::string_view text :
                     {"ASCII", "cafe\xcc\x81", "中文", "👩‍💻", "🇺🇸", "→ ─ ╭╮"}) {
                    auto cells = cell_width(text).value_or(0);
                    line("| " + std::string{text} +
                         std::string(static_cast<std::size_t>(12 - cells), ' ') + "| " +
                         std::to_string(cells) + " cells");
                }
                break;
            case 5:
                heading("LIVE INPUT");
                line(std::string{"m mouse "} + (mouse ? "ON" : "off") + "   f focus " +
                     (focus ? "ON" : "off"));
                line(std::string{"v paste "} + (paste ? "ON" : "off") +
                     "   k Kitty negotiation");
                line("Move, click, scroll, paste, or change focus.");
                ++y;
                for (auto const& e : events)
                    if (y < size.height - 5) line(e);
                break;
            case 6:
                heading("CURSOR CONTROLS");
                line("c cycle cursor shape   h toggle visibility");
                line("s save/restore   e editing   r scrolling");
                line("Shape: " + std::string{cursor_names[cursor_shape]});
                ++y;
                cursor_y = y;
                line("Cursor target: [ ]");
                if (control_sample == 's') {
                    append_encoded(frame, Cursor{x, y + 1}, SaveCursor{},
                                   Cursor{x + 10, y + 2}, "decoy", RestoreCursor{},
                                   "PASS");
                    y += 3;
                }
                else if (control_sample == 'e') {
                    row(x, y++, "Expect AC!D below:");
                    append_encoded(frame, Cursor{x, y}, "ABCD", Cursor{x + 1, y},
                                   DeleteCharacters{1}, Cursor{x + 2, y},
                                   InsertCharacters{1}, "!");
                }
                else if (control_sample == 'r') {
                    row(x, y++, "Expect row 2 / row 3 / tail below:");
                    row(x, y, "row 1");
                    row(x, y + 1, "row 2");
                    row(x, y + 2, "row 3");
                    append_encoded(frame, ScrollRegion{y, y + 2}, ScrollUp{1},
                                   Cursor{x, y + 2}, "tail", ResetScrollRegion{});
                }
                break;
            case 7:
                heading("OSC 8 HYPERLINKS");
                line("Use your terminal's modifier + click gesture.");
                ++y;
                append_encoded(
                    frame, Cursor{x, y++},
                    BeginHyperlink{"https://github.com/a-n-t-h-o-n-y/Escape"},
                    "Open the Escape repository", EndHyperlink{});
                ++y;
                line("The text should behave as a link.");
                line("Adjacent plain text should not be linked.");
                break;
            case 8:
                heading("BELL / DECPS SOUND");
                line("b request the configured terminal bell");
                line("n play a C5 - E5 - G5 - C6 chime");
                ++y;
                line("Bell may beep, flash, or be muted.");
                line("DECPS has limited terminal support.");
                line("No sound is played automatically.");
                break;
            case 9:
                heading("RESULTS / TERMINAL REPLIES");
                line("p modes  k Kitty  a profile assumptions");
                line("t device ID  z sync  Up / Down scroll");
                ++y;
                if (show_assumptions) {
                    line("ENVIRONMENT ASSUMPTIONS (not observations)", bold);
                    for (unsigned i = report_offset;
                         i < assumptions.decisions.size() && y < size.height - 5; ++i) {
                        auto const& d = assumptions.decisions[i];
                        line(std::string{features[static_cast<unsigned>(d.feature)]});
                        line("  " + d.explanation);
                    }
                }
                else {
                    for (unsigned i = report_offset;
                         i < probes.size() && y < size.height - 5; ++i)
                        line(probes[i]);
                    if (probes.empty()) line("No queries yet. Timeout means unknown.");
                }
                break;
        }
    }

   public:
    Tester(Terminal& t, Environment e)
        : terminal{t},
          env{std::move(e)},
          assumptions{resolve_profile(env, true)},
          chrome_color{!env.no_color}
    {}
    void draw()
    {
        size = terminal.area();
        frame.clear();
        append_encoded(frame, ResetStyle{}, BlankScreen{}, Cursor{});
        if (size.width < 70 || size.height < 24) {
            row(1, 1, "ESCAPE / TERMCAPS", bold);
            row(1, 3, "Resize to at least 70 x 24 for the tester.");
            row(1, 5, "q quit | arrows change page");
            terminal.output().write(frame);
            terminal.flush();
            return;
        }
        width = size.width - x - 2;
        y = 7;
        row(2, 1, "ESCAPE  /  TERMCAPS", bold, true);
        row(size.width - 22, 1,
            std::to_string(size.width) + " x " + std::to_string(size.height));
        row(2, 3, "TERM " + safe(env.term) + "  |  COLORTERM " + safe(env.colorterm));
        row(2, 4, "Raw samples   /   input events   /   terminal replies", dim);
        row(2, 5, std::string(static_cast<std::size_t>(size.width - 4), '-'), dim);
        body();
        for (unsigned i = 0; i < pages.size(); ++i) {
            std::string label =
                (i == 9 ? "0" : std::to_string(i + 1)) + " " + std::string{pages[i]};
            label = fit(label, 19);
            row(2, 7 + static_cast<int>(i), label,
                i == page ? Traits{inverse} : Traits{});
        }
        row(2, size.height - 4, message, {}, true);
        row(2, size.height - 3, "Arrows / 1-0 pages   p probe   q quit", bold);
        row(2, size.height - 2, "Page: " + std::string{pages[page]});
        // Full styled rows restore defaults; samples deliberately bypass fallback.
        if (cursor_visible && page == 6)
            append_encoded(frame, Cursor{x + 16, cursor_y});
        terminal.output().write(frame);
        terminal.flush();
    }
    void query_modes()
    {
        probes.clear();
        show_assumptions = false;
        report_offset = 0;
        for (auto const& o : terminal.probe(350ms).observations) {
            auto label = match(
                o.request, [](DeviceAttributesQuery) { return std::string{"DA"}; },
                [](KeyboardQuery) { return std::string{"Keyboard"}; },
                [](ModeQuery q) { return "Mode " + std::to_string(q.mode); });
            probes.push_back(label + " / " + outcome(o.result));
        }
        page = 9;
        message = "Terminal replies recorded on the probe page.";
    }
    void kitty()
    {
        auto r = terminal.enable_keyboard(
            KittyFlags::Disambiguate | KittyFlags::EventTypes |
                KittyFlags::AlternateKeys | KittyFlags::AllKeys |
                KittyFlags::AssociatedText,
            350ms);
        message = "Keyboard negotiation: " + outcome(r);
        probes.push_back(message);
        if (probes.size() > 12) probes.erase(probes.begin());
    }
    bool handle(Event const& event)
    {
        events.push_front(event_text(event));
        if (events.size() > 8) events.pop_back();
        auto key = std::get_if<KeyEvent>(&event);
        if (!key || key->action == KeyAction::Release) return true;
        if (key->matches(U'q') || key->matches(U'c', Modifiers{.ctrl = true}) ||
            key->matches(U'\x03'))
            return false;
        auto old_page = page;
        if (key->matches(KeyCode::ArrowDown) || key->matches(KeyCode::PageDown)) {
            if (page == 1) palette_offset = (std::min)(15u, palette_offset + 1);
            if (page == 9)
                report_offset =
                    (std::min)(show_assumptions
                                   ? 16u
                                   : (probes.empty()
                                          ? 0u
                                          : static_cast<unsigned>(probes.size() - 1)),
                               report_offset + 1);
        }
        else if (key->matches(KeyCode::ArrowUp) || key->matches(KeyCode::PageUp)) {
            if (page == 1 && palette_offset) --palette_offset;
            if (page == 9 && report_offset) --report_offset;
        }
        else if (key->matches(KeyCode::ArrowRight) || key->matches(KeyCode::Tab))
            page = (page + 1) % 10;
        else if (key->matches(KeyCode::ArrowLeft))
            page = (page + 9) % 10;
        else if (key->key) {
            auto c = text_scalar(*key->key).value_or(0);
            if (key->modifiers != Modifiers{}) return true;
            if (c >= '1' && c <= '9')
                page = static_cast<unsigned>(c - '1');
            else if (c == '0')
                page = 9;
            else if (c == 'p')
                query_modes();
            else if (c == 'k')
                kitty();
            else if (c == 'a') {
                show_assumptions = !show_assumptions;
                report_offset = 0;
                page = 9;
            }
            else if (c == 't' || c == 'z') {
                auto r = c == 't' ? terminal.query(DeviceAttributesQuery{}, 350ms)
                                  : terminal.confirm_synchronized_output(350ms);
                probes.push_back(outcome(r));
                if (probes.size() > 12) probes.erase(probes.begin());
                page = 9;
                show_assumptions = false;
                report_offset = 0;
                message = probes.back();
            }
            else if (page == 5 && c == 'm') {
                mouse = !mouse;
                terminal.set(mouse ? MouseMode::Move : MouseMode::Off);
            }
            else if (page == 5 && c == 'f') {
                focus = !focus;
                terminal.set(focus ? FocusReporting::On : FocusReporting::Off);
            }
            else if (page == 5 && c == 'v') {
                paste = !paste;
                terminal.set(paste ? BracketedPaste::On : BracketedPaste::Off);
            }
            else if (page == 8 && c == 'b') {
                terminal.output().write(Bell{});
                terminal.flush();
                message = "Bell sent.";
            }
            else if (page == 8 && c == 'n') {
                terminal.output().write(
                    PlayNotes{.notes = {Note::C5, Note::E5, Note::G5, Note::C6},
                              .duration = 125ms});
                terminal.flush();
                message = "DECPS chime sent.";
            }
            else if (page == 6 && c == 'h') {
                cursor_visible = !cursor_visible;
                terminal.set(cursor_visible ? CursorMode::Show : CursorMode::Hide);
            }
            else if (page == 6 && c == 'c') {
                cursor_shape = (cursor_shape + 1) % cursor_styles.size();
                cursor_visible = true;
                terminal.set(CursorMode::Show);
                terminal.output().write(cursor_styles[cursor_shape]);
                terminal.flush();
                message = std::string{cursor_names[cursor_shape]};
            }
            else if (page == 6 && (c == 's' || c == 'e' || c == 'r')) {
                control_sample = static_cast<char>(c);
                message = "Compare the control sample with its expected result.";
            }
        }
        if (old_page != 6 && page == 6) {
            cursor_shape = 0;
            cursor_visible = true;
            terminal.set(CursorMode::Show);
            terminal.output().write(cursor_styles[cursor_shape]);
            terminal.flush();
        }
        if (old_page == 6 && page != 6) {
            cursor_visible = false;
            terminal.set(CursorMode::Hide);
            terminal.output().write(CursorStyle::Default);
            terminal.flush();
        }
        return true;
    }
};
struct VisualCleanup {
    Terminal& terminal;
    ~VisualCleanup()
    {
        try {
            terminal.output().write(ResetScrollRegion{}, CursorStyle::Default,
                                    ResetStyle{});
            terminal.flush();
        }
        catch (...) {
        }
    }
};
}  // namespace
int main(int argc, char** argv)
{
    try {
        bool plain = false, probe = false, kitty = false, force = false;
        for (int i = 1; i < argc; ++i) {
            std::string_view arg{argv[i]};
            if (arg == "--report")
                plain = true;
            else if (arg == "--probe")
                probe = true;
            else if (arg == "--kitty")
                kitty = true;
            else if (arg == "--force")
                force = true;
            else if (arg == "--help" || arg == "-h") {
                std::cout
                    << "Usage: termcaps [--report] [--probe] [--kitty] [--force]\n"
                       "Interactive capability tester. --report prints assumptions "
                       "without control bytes.\n"
                       "--probe queries at startup; --kitty negotiates extended "
                       "input.\n"
                       "--force permits interactive testing with TERM=dumb/empty (TTY "
                       "required).\n";
                return 0;
            }
            else
                throw std::invalid_argument{"Unknown option; use termcaps --help"};
        }
        auto env = snapshot_environment();
        if (plain || !tty(stdin) || !tty(stdout) ||
            (!force && (env.term.empty() || env.term == "dumb"))) {
            report(env, tty(stdout));
            return 0;
        }
        // Testing opts into protocols rather than hiding them behind fallback.
        // The report separately retains the actual environment assumptions.
        TerminalProfile profile;
        profile.control = true;
        profile.alternate_screen = true;
        profile.cursor_visibility = true;
        profile.mouse_protocol = MouseProtocol::Sgr;
        profile.maximum_mouse_mode = MouseMode::Move;
        profile.focus_reporting = true;
        profile.bracketed_paste = true;
        auto options = TerminalOptions::interactive();
        options.profile = profile;
        options.signals = Signals::Off;
        auto terminal = Terminal::open(options);
        Tester tester{terminal, std::move(env)};
        {
            VisualCleanup cleanup{terminal};
            tester.draw();
            if (probe) tester.query_modes();
            if (kitty) tester.kitty();
            tester.draw();
            bool dirty = false;
            auto last = std::chrono::steady_clock::now();
            for (;;) {
                auto r = terminal.read(50ms);
                if (std::holds_alternative<EndOfInput>(r)) break;
                if (auto e = std::get_if<Event>(&r)) {
                    if (!tester.handle(*e)) break;
                    dirty = true;
                }
                auto now = std::chrono::steady_clock::now();
                if (dirty && now - last >= 50ms) {
                    tester.draw();
                    dirty = false;
                    last = now;
                }
            }
        }
        terminal.close();
        return 0;
    }
    catch (std::exception const& e) {
        std::cerr << "termcaps: " << e.what() << '\n';
        return 1;
    }
}
