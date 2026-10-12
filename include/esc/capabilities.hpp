#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include <esc/sequence.hpp>

namespace esc {

enum class Feature { Control, Colors, Traits, AlternateScreen, CursorVisibility, Mouse, Focus, BracketedPaste, CursorStyle, CursorSaveRestore, Scrolling, Editing, WindowTitle, Hyperlinks, SynchronizedOutput };
class UnsupportedFeature : public std::runtime_error {
public:
    explicit UnsupportedFeature(Feature feature);
    [[nodiscard]] auto feature() const noexcept -> Feature { return feature_; }
private:
    Feature feature_;
};
enum class ColorSupport { None, Basic8, Basic16, Indexed256, TrueColor };
enum class FallbackPolicy { BestEffort, Strict };

[[nodiscard]] constexpr auto conventional_system_palette() -> std::array<RGB, 16>
{
    return {RGB{0x000000},RGB{0x800000},RGB{0x008000},RGB{0x808000},
            RGB{0x000080},RGB{0x800080},RGB{0x008080},RGB{0xC0C0C0},
            RGB{0x808080},RGB{0xFF0000},RGB{0x00FF00},RGB{0xFFFF00},
            RGB{0x0000FF},RGB{0xFF00FF},RGB{0x00FFFF},RGB{0xFFFFFF}};
}

// Caller-owned capability assumptions. A default profile emits plain output.
// The palette guides approximation; it does not change the terminal's palette.
struct TerminalProfile {
    bool control = false;
    ColorSupport colors = ColorSupport::None;
    Traits traits = {};
    bool alternate_screen = false;
    bool cursor_visibility = false;
    MouseProtocol mouse_protocol = MouseProtocol::None;
    MouseMode maximum_mouse_mode = MouseMode::Off;
    bool focus_reporting = false;
    bool bracketed_paste = false;
    FallbackPolicy fallback = FallbackPolicy::BestEffort;
    std::array<RGB, 16> system_palette = conventional_system_palette();
    bool cursor_style = false;
    bool cursor_save_restore = false;
    bool scrolling = false;
    bool editing = false;
    bool window_title = false;
    bool hyperlinks = false;
    bool synchronized_output = false;
};

struct Environment {
    std::string term;
    std::string colorterm;
    bool no_color = false;
};
enum class EvidenceSource { Default, Environment, KnownTerminal, Explicit, Probe };
struct CapabilityDecision {
    Feature feature;
    EvidenceSource source;
    std::string explanation;
};
struct ProfileReport {
    TerminalProfile profile;
    std::vector<CapabilityDecision> decisions;
};

// Snapshot is the explicit environment-reading boundary. The resolver below is
// pure and uses exact TERM names from the versioned table in the implementation.
[[nodiscard]] auto snapshot_environment() -> Environment;
[[nodiscard]] auto resolve_profile(Environment const& environment, bool output_is_terminal) -> ProfileReport;

} // namespace esc
