#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include <esc/sequence.hpp>

namespace esc {

/// Features whose support can be requested or rejected by a profile.
enum class Feature {
    Control,
    Colors,
    Traits,
    AlternateScreen,
    CursorVisibility,
    Mouse,
    Focus,
    BracketedPaste,
    CursorStyle,
    CursorSaveRestore,
    Scrolling,
    Editing,
    WindowTitle,
    Hyperlinks,
    SynchronizedOutput,
    Bell,
    PlayNotes
};

/// Strict policy rejected a feature. Inspect feature() for the failed request.
class UnsupportedFeature : public std::runtime_error {
   public:
    explicit UnsupportedFeature(Feature feature);

    UnsupportedFeature(Feature feature, std::string context);

    /// Return the unsupported feature carried by this error.
    [[nodiscard]] auto feature() const noexcept -> Feature { return feature_; }

   private:
    Feature feature_;
};

/// Maximum color depth, from plain output to 24-bit RGB.
enum class ColorSupport { None, Basic8, Basic16, Indexed256, TrueColor };

/// Approximate or omit unsupported output, or reject it before encoding.
enum class FallbackPolicy { BestEffort, Strict };

/// Return conventional RGB estimates for the sixteen system palette entries.
[[nodiscard]] constexpr auto conventional_system_palette() -> std::array<RGB, 16>
{
    return {RGB{0x000000}, RGB{0x800000}, RGB{0x008000}, RGB{0x808000},
            RGB{0x000080}, RGB{0x800080}, RGB{0x008080}, RGB{0xC0C0C0},
            RGB{0x808080}, RGB{0xFF0000}, RGB{0x00FF00}, RGB{0xFFFF00},
            RGB{0x0000FF}, RGB{0xFF00FF}, RGB{0x00FFFF}, RGB{0xFFFFFF}};
}

/// Caller-owned capability assumptions. A default profile emits plain output.
/// The palette guides approximation; it does not change the terminal's palette.
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
    // Bell is independent of cursor/color control. Playback requires DECPS.
    bool bell = false;
    bool play_notes = false;
};

/// Owned TERM, COLORTERM, and NO_COLOR snapshot for pure resolution.
struct Environment {
    std::string term;
    std::string colorterm;
    bool no_color = false;
};

enum class EvidenceSource {
    Default,
    Environment,
    KnownTerminal,
    TerminalFamily,
    Explicit,
    Probe
};

struct CapabilityDecision {
    Feature feature;
    EvidenceSource source;
    std::string explanation;
};

/// Resolved assumptions and the evidence used for each decision.
struct ProfileReport {
    TerminalProfile profile;
    std::vector<CapabilityDecision> decisions;
};

/// Snapshot is the explicit environment-reading boundary. The resolver below is
/// pure: exact TERM entries take priority over the conservative xterm-* baseline.
[[nodiscard]] auto snapshot_environment() -> Environment;

/// Resolve owned environment values without I/O. Nonterminal output is plain.
[[nodiscard]] auto resolve_profile(Environment const& environment,
                                   bool output_is_terminal) -> ProfileReport;

}  // namespace esc
