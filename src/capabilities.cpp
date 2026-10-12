#include <esc/capabilities.hpp>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace esc {
namespace {
auto unsupported_message(Feature feature) -> char const*
{
    switch (feature) {
        case Feature::Control:
            return "Terminal profile does not support cursor control";
        case Feature::Colors:
            return "Terminal profile does not support the requested color";
        case Feature::Traits:
            return "Terminal profile does not support the requested traits";
        case Feature::AlternateScreen:
            return "Terminal profile does not support alternate screen mode";
        case Feature::CursorVisibility:
            return "Terminal profile does not support cursor visibility";
        case Feature::Mouse:
            return "Terminal profile does not support the requested mouse reporting";
        case Feature::Focus: return "Terminal profile does not support focus reporting";
        case Feature::BracketedPaste:
            return "Terminal profile does not support bracketed paste";
        case Feature::CursorStyle:
            return "Terminal profile does not support cursor style";
        case Feature::CursorSaveRestore:
            return "Terminal profile does not support cursor save/restore";
        case Feature::Scrolling: return "Terminal profile does not support scrolling";
        case Feature::Editing:
            return "Terminal profile does not support line/character editing";
        case Feature::WindowTitle:
            return "Terminal profile does not support window titles";
        case Feature::Hyperlinks: return "Terminal profile does not support hyperlinks";
        case Feature::Bell: return "Terminal profile does not support the bell";
        case Feature::PlayNotes:
            return "Terminal profile does not support DECPS playback";
        case Feature::SynchronizedOutput:
            return "Terminal profile does not support synchronized output";
    }
    throw std::invalid_argument{"UnsupportedFeature: invalid feature"};
}

constexpr auto basic_traits() -> Traits
{
    return Trait::Bold | Trait::Dim | Trait::Underline | Trait::Blink | Trait::Inverse |
           Trait::Invisible;
}

constexpr auto full_traits() -> Traits
{ return basic_traits() | Trait::Italic | Trait::CrossedOut | Trait::DoubleUnderline; }

struct KnownTerminal {
    std::string_view term;
    ColorSupport colors;
    bool full;
    bool alternate;
    bool visibility;
};

// Resolution table v1, matching interface-proposal.md Appendix C. This is a
// reviewed advertised-protocol heuristic, not a captured-emulator claim.
// Protocol basis: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
// Mux input features stay conservative; do not infer the terminal behind a mux.
constexpr KnownTerminal known_terminals[] = {
    {"ansi", ColorSupport::Basic8, false, false, false},
    {"xterm", ColorSupport::Basic8, true, true, true},
    {"xterm-color", ColorSupport::Basic8, true, true, true},
    {"xterm-256color", ColorSupport::Indexed256, true, true, true},
    {"screen", ColorSupport::Basic8, false, true, true},
    {"tmux", ColorSupport::Basic8, false, true, true},
    {"screen-256color", ColorSupport::Indexed256, false, true, true},
    {"tmux-256color", ColorSupport::Indexed256, false, true, true},
    {"linux", ColorSupport::Basic16, false, false, true},
};
}  // namespace

UnsupportedFeature::UnsupportedFeature(Feature feature)
    : std::runtime_error{unsupported_message(feature)}, feature_{feature}
{}

auto snapshot_environment() -> Environment
{
    auto read = [](char const* name) -> std::string {
        auto const* value = std::getenv(name);
        return value ? value : "";
    };
    auto const* no_color = std::getenv("NO_COLOR");
    return {read("TERM"), read("COLORTERM"), no_color && *no_color};
}

auto resolve_profile(Environment const& environment, bool output_is_terminal)
    -> ProfileReport
{
    ProfileReport report;
    auto source = EvidenceSource::Default;
    std::string explanation =
        "Plain fallback: TERM is empty, dumb, or not in resolution table v1";
    if (!output_is_terminal) explanation = "Plain fallback: output is not a terminal";
    if (output_is_terminal) {
        for (auto const& entry : known_terminals) {
            if (environment.term != entry.term) continue;
            report.profile = {
                .control = true,
                .colors = entry.colors,
                .traits = entry.full ? full_traits() : basic_traits(),
                .alternate_screen = entry.alternate,
                .cursor_visibility = entry.visibility,
                .mouse_protocol = entry.full ? MouseProtocol::Sgr : MouseProtocol::None,
                .maximum_mouse_mode = entry.full ? MouseMode::Move : MouseMode::Off,
                .focus_reporting = entry.full,
                .bracketed_paste = entry.full,
                .bell = true};
            source = EvidenceSource::KnownTerminal;
            explanation =
                "Advertised-protocol heuristic from exact TERM match in table v1: " +
                environment.term;
            break;
        }
    }
    auto color_source = source;
    auto color_explanation = explanation;
    if (report.profile.control &&
        (environment.colorterm == "truecolor" || environment.colorterm == "24bit")) {
        report.profile.colors = ColorSupport::TrueColor;
        color_source = EvidenceSource::Environment;
        color_explanation =
            "COLORTERM advertises true color for a recognized control-capable TERM";
    }
    if (environment.no_color) {
        report.profile.colors = ColorSupport::None;
        color_source = EvidenceSource::Environment;
        color_explanation =
            "NO_COLOR suppresses colors; other profile features remain unchanged";
    }
    constexpr Feature features[] = {
        Feature::Control,         Feature::Colors,           Feature::Traits,
        Feature::AlternateScreen, Feature::CursorVisibility, Feature::Mouse,
        Feature::Focus,           Feature::BracketedPaste};
    report.decisions.reserve(std::size(features) + 9);
    for (auto feature : features) {
        bool const color = feature == Feature::Colors;
        report.decisions.push_back({feature, color ? color_source : source,
                                    color ? color_explanation : explanation});
    }
    for (auto feature : {Feature::CursorStyle, Feature::CursorSaveRestore,
                         Feature::Scrolling, Feature::Editing, Feature::WindowTitle,
                         Feature::Hyperlinks, Feature::SynchronizedOutput})
        report.decisions.push_back({feature, EvidenceSource::Default,
                                    "Extension support requires an explicit profile or "
                                    "a supported explicit probe"});
    report.decisions.push_back({Feature::Bell, source, explanation});
    report.decisions.push_back({Feature::PlayNotes, EvidenceSource::Default,
                                "DECPS playback requires an explicit profile"});
    return report;
}
}  // namespace esc

namespace esc::detail {
namespace {
void require(bool supported, Feature feature)
{
    if (!supported) throw UnsupportedFeature{feature};
}

[[nodiscard]] auto palette_color(TerminalProfile const& profile, unsigned index) -> RGB
{
    if (index < 16) return profile.system_palette[index];
    if (index >= 232) {
        auto const gray = static_cast<std::uint8_t>(8 + 10 * (index - 232));
        return {gray, gray, gray};
    }
    constexpr std::uint8_t levels[] = {0, 95, 135, 175, 215, 255};
    auto const cube = index - 16;
    return {levels[cube / 36], levels[(cube / 6) % 6], levels[cube % 6]};
}

[[nodiscard]] auto nearest_color(TerminalProfile const& profile, RGB color)
    -> IndexedColor
{
    unsigned const candidates = profile.colors == ColorSupport::Basic8 ? 8 : 16;
    unsigned best = 0;
    unsigned distance = std::numeric_limits<unsigned>::max();
    auto consider = [&](unsigned index, RGB candidate) {
        int const red = int(color.red) - int(candidate.red);
        int const green = int(color.green) - int(candidate.green);
        int const blue = int(color.blue) - int(candidate.blue);
        unsigned const score =
            static_cast<unsigned>(red * red + green * green + blue * blue);
        if (score < distance) {
            best = index;
            distance = score;
        }
    };
    for (unsigned i = 0; i < candidates; ++i)
        consider(i, profile.system_palette[i]);
    if (profile.colors == ColorSupport::Indexed256) {
        // Squared RGB distance is separable on the xterm cube. The nearest
        // channel in each axis gives the nearest of all 216 cube entries.
        // Midpoint ties choose the lower channel/index.
        auto channel = [](unsigned value) -> unsigned {
            return value <= 47    ? 0
                   : value <= 115 ? 1
                   : value <= 155 ? 2
                   : value <= 195 ? 3
                   : value <= 235 ? 4
                                  : 5;
        };
        auto cube = 16 + 36 * channel(color.red) + 6 * channel(color.green) +
                    channel(color.blue);
        consider(cube, palette_color(profile, cube));
        // Gray minimizes the same metric at the mean of the three channels.
        // Work with their sum to keep rounding exact; ties choose lower gray.
        auto sum = unsigned(color.red) + color.green + color.blue;
        auto gray = 232 + (sum <= 24 ? 0 : std::min(23u, (sum - 24 + 14) / 30));
        consider(gray, palette_color(profile, gray));
    }
    return {static_cast<std::uint8_t>(best)};
}

[[nodiscard]] auto prepare_color(TerminalProfile const& profile, Color const& color)
    -> std::optional<Color>
{
    validate_color(color);
    if (std::holds_alternative<DefaultColor>(color))
        return profile.colors == ColorSupport::None ? std::nullopt
                                                    : std::optional<Color>{color};
    if (profile.colors == ColorSupport::None) {
        if (profile.fallback == FallbackPolicy::Strict)
            throw UnsupportedFeature{Feature::Colors};
        return std::nullopt;
    }
    if (auto const* indexed = std::get_if<IndexedColor>(&color)) {
        if (profile.colors == ColorSupport::Indexed256 ||
            profile.colors == ColorSupport::TrueColor ||
            indexed->value < (profile.colors == ColorSupport::Basic8 ? 8 : 16))
            return color;
        if (profile.fallback == FallbackPolicy::Strict)
            throw UnsupportedFeature{Feature::Colors};
        if (profile.colors == ColorSupport::Basic8 && indexed->value < 16)
            return Color{IndexedColor{static_cast<std::uint8_t>(indexed->value - 8)}};
        return Color{nearest_color(profile, palette_color(profile, indexed->value))};
    }
    if (profile.colors == ColorSupport::TrueColor) return color;
    if (profile.fallback == FallbackPolicy::Strict)
        throw UnsupportedFeature{Feature::Colors};
    auto const true_color = std::get<RGB>(color);
    return Color{
        nearest_color(profile, RGB{true_color.red, true_color.green, true_color.blue})};
}

void validate_erase(Erase erase)
{
    switch (erase) {
        case Erase::ToEnd:
        case Erase::ToBeginning:
        case Erase::All: return;
    }
    throw std::invalid_argument{"escape: invalid erase extent"};
}
}  // namespace

void validate_profile(TerminalProfile const& profile)
{
    switch (profile.colors) {
        case ColorSupport::None:
        case ColorSupport::Basic8:
        case ColorSupport::Basic16:
        case ColorSupport::Indexed256:
        case ColorSupport::TrueColor: break;
        default: throw std::invalid_argument{"TerminalProfile: invalid color support"};
    }
    switch (profile.fallback) {
        case FallbackPolicy::BestEffort:
        case FallbackPolicy::Strict: break;
        default:
            throw std::invalid_argument{"TerminalProfile: invalid fallback policy"};
    }
    switch (profile.mouse_protocol) {
        case MouseProtocol::None:
        case MouseProtocol::Legacy:
        case MouseProtocol::Sgr:
        case MouseProtocol::Urxvt: break;
        default: throw std::invalid_argument{"TerminalProfile: invalid mouse protocol"};
    }
    switch (profile.maximum_mouse_mode) {
        case MouseMode::Off:
        case MouseMode::Basic:
        case MouseMode::Drag:
        case MouseMode::Move: break;
        default:
            throw std::invalid_argument{"TerminalProfile: invalid maximum mouse mode"};
    }
    if ((profile.mouse_protocol == MouseProtocol::None) !=
        (profile.maximum_mouse_mode == MouseMode::Off))
        throw std::invalid_argument{
            "TerminalProfile: mouse protocol and tracking limit are inconsistent"};
    if (!profile.control &&
        (profile.colors != ColorSupport::None || profile.traits.data() ||
         profile.alternate_screen || profile.cursor_visibility ||
         profile.mouse_protocol != MouseProtocol::None || profile.focus_reporting ||
         profile.bracketed_paste || profile.cursor_style ||
         profile.cursor_save_restore || profile.scrolling || profile.editing ||
         profile.window_title || profile.hyperlinks || profile.synchronized_output ||
         profile.play_notes))
        throw std::invalid_argument{
            "TerminalProfile: plain output cannot have control features"};
}

void encode(SequenceWriter& output, PreparedStyle const& style)
{
    encode(output, style.background);
    encode(output, style.foreground);
    if (style.traits) replace_traits(output, *style.traits);
}

auto prepare(TerminalProfile const& profile, Cursor const& cursor)
    -> std::optional<Cursor>
{
    if (cursor.x < 0 || cursor.y < 0)
        throw std::invalid_argument{"escape: cursor coordinates must be nonnegative"};
    require(profile.control, Feature::Control);
    return cursor;
}

auto prepare(TerminalProfile const& profile, MoveCursor const& cursor)
    -> std::optional<MoveCursor>
{
    require(profile.control, Feature::Control);
    return cursor;
}

auto prepare(TerminalProfile const& profile, BlankRow const& row)
    -> std::optional<BlankRow>
{
    validate_erase(row.extent);
    require(profile.control, Feature::Control);
    return row;
}

auto prepare(TerminalProfile const& profile, BlankScreen const& screen)
    -> std::optional<BlankScreen>
{
    validate_erase(screen.extent);
    require(profile.control, Feature::Control);
    return screen;
}

auto prepare(TerminalProfile const& profile, ResetStyle const& reset)
    -> std::optional<ResetStyle>
{ return profile.control ? std::optional<ResetStyle>{reset} : std::nullopt; }

auto prepare(TerminalProfile const& profile, Traits const& traits)
    -> std::optional<Traits>
{
    auto const missing = traits.data() & ~profile.traits.data();
    if (missing && profile.fallback == FallbackPolicy::Strict)
        throw UnsupportedFeature{Feature::Traits};
    if (!profile.traits.data()) return std::nullopt;
    auto resolved = traits;
    // Remove unsupported flags; the type validates their combined modeled mask.
    resolved.remove(static_cast<Trait>(missing));
    if (traits.contains(Trait::DoubleUnderline) &&
        !profile.traits.contains(Trait::DoubleUnderline) &&
        profile.traits.contains(Trait::Underline))
        resolved.insert(Trait::Underline);
    return resolved;
}

auto prepare(TerminalProfile const& profile, Trait trait) -> std::optional<Traits>
{ return prepare(profile, Traits{trait}); }

auto prepare(TerminalProfile const& profile, ColorFG const& foreground)
    -> std::optional<ColorFG>
{
    auto color = prepare_color(profile, foreground.value);
    return color ? std::optional<ColorFG>{{*color}} : std::nullopt;
}

auto prepare(TerminalProfile const& profile, ColorBG const& background)
    -> std::optional<ColorBG>
{
    auto color = prepare_color(profile, background.value);
    return color ? std::optional<ColorBG>{{*color}} : std::nullopt;
}

auto prepare(TerminalProfile const& profile, Style const& style) -> PreparedStyle
{
    validate_style(style);
    return {prepare(profile, bg(style.background)),
            prepare(profile, fg(style.foreground)), prepare(profile, style.traits)};
}

auto prepare(TerminalProfile const& profile, ScreenBuffer const& screen)
    -> std::optional<ScreenBuffer>
{
    if (screen != ScreenBuffer::Normal && screen != ScreenBuffer::Alternate)
        throw std::invalid_argument{"escape: invalid screen buffer"};
    require(profile.alternate_screen, Feature::AlternateScreen);
    return screen;
}

auto prepare(TerminalProfile const& profile, CursorMode const& cursor)
    -> std::optional<CursorMode>
{
    if (cursor != CursorMode::Show && cursor != CursorMode::Hide)
        throw std::invalid_argument{"escape: invalid cursor mode"};
    require(profile.cursor_visibility, Feature::CursorVisibility);
    return cursor;
}

auto prepare(TerminalProfile const& profile, FocusReporting const& focus)
    -> std::optional<FocusReporting>
{
    if (focus != FocusReporting::Off && focus != FocusReporting::On)
        throw std::invalid_argument{"escape: invalid focus reporting"};
    require(profile.focus_reporting, Feature::Focus);
    return focus;
}

auto prepare(TerminalProfile const& profile, BracketedPaste const& paste)
    -> std::optional<BracketedPaste>
{
    if (paste != BracketedPaste::Off && paste != BracketedPaste::On)
        throw std::invalid_argument{"escape: invalid bracketed paste"};
    require(profile.bracketed_paste, Feature::BracketedPaste);
    return paste;
}

auto prepare(TerminalProfile const& profile, MouseReporting const& mouse)
    -> std::optional<MouseReporting>
{
    switch (mouse.mode) {
        case MouseMode::Off:
        case MouseMode::Basic:
        case MouseMode::Drag:
        case MouseMode::Move: break;
        default: throw std::invalid_argument{"escape: invalid mouse mode"};
    }
    switch (mouse.protocol) {
        case MouseProtocol::None:
        case MouseProtocol::Legacy:
        case MouseProtocol::Sgr:
        case MouseProtocol::Urxvt: break;
        default: throw std::invalid_argument{"escape: invalid mouse protocol"};
    }
    if (mouse.mode != MouseMode::Off && mouse.protocol == MouseProtocol::None)
        throw std::invalid_argument{"escape: enabled mouse needs a protocol"};
    require(profile.mouse_protocol != MouseProtocol::None, Feature::Mouse);
    if (mouse.mode == MouseMode::Off) return mouse;
    require(mouse.mode <= profile.maximum_mouse_mode, Feature::Mouse);
    if (mouse.protocol != profile.mouse_protocol &&
        profile.fallback == FallbackPolicy::Strict)
        throw UnsupportedFeature{Feature::Mouse};
    return MouseReporting{mouse.mode, profile.mouse_protocol};
}
}  // namespace esc::detail
