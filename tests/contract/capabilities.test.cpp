#include "support/capabilities.hpp"

#include <cmath>
#include <stdexcept>

using namespace esc;
using contract::encodes;

TEST(CAP_01_plain_output_drops_style_but_rejects_geometry)
{
    TerminalProfile plain;
    encodes("",plain,fg(XColor::Red),bg(TColor{0x010203}),Trait::Bold,Brush{},ResetStyle{});
    CHECK_THROWS(UnsupportedFeature,escape(plain,Cursor{}));
    CHECK_THROWS(UnsupportedFeature,escape(plain,MoveCursor{1,0}));
    CHECK_THROWS(UnsupportedFeature,escape(plain,BlankRow{}));
    CHECK_THROWS(UnsupportedFeature,escape(plain,ScreenBuffer::Alternate));
}
TEST(CAP_01_strict_plain_profile_accepts_only_default_style)
{
    TerminalProfile plain;
    plain.fallback=FallbackPolicy::Strict;
    encodes("",plain,Brush{},Traits{},fg(TermColor::Default),ResetStyle{});
    CHECK_THROWS(UnsupportedFeature,escape(plain,fg(XColor::Red)));
    CHECK_THROWS(UnsupportedFeature,escape(plain,Trait::Bold));
}
TEST(CAP_01_supported_commands_match_the_raw_contract)
{
    auto profile=contract::full_profile();
    encodes("\x1b[3;5H\x1b[38;2;1;2;3m",profile,Cursor{4,2},fg(TColor{0x010203}));
    encodes("\x1b[?1049h",profile,ScreenBuffer::Alternate);
    encodes("\x1b[?1004h",profile,FocusReporting::On);
    encodes("\x1b[?2004h",profile,BracketedPaste::On);
}
TEST(CAP_01_unsupported_traits_fallback_or_throw)
{
    auto profile=contract::full_profile();
    profile.traits=Trait::Bold | Trait::Underline;
    encodes("\x1b[22;23;24;25;27;28;29;1m",profile,Trait::Bold | Trait::Italic);
    encodes("\x1b[22;23;24;25;27;28;29;4m",profile,Trait::DoubleUnderline);
    encodes("\x1b[22;23;24;25;27;28;29;4m",profile,Trait::Underline | Trait::DoubleUnderline);
    profile.fallback=FallbackPolicy::Strict;
    CHECK_THROWS(UnsupportedFeature,escape(profile,Trait::Italic));
    CHECK_THROWS(UnsupportedFeature,escape(profile,Trait::DoubleUnderline));
    encodes("\x1b[22;23;24;25;27;28;29m",profile,Traits{});
}
TEST(CAP_01_no_supported_traits_means_no_trait_bytes)
{
    auto profile=contract::full_profile();
    profile.traits={};
    encodes("",profile,Trait::Bold,Traits{});
    profile.fallback=FallbackPolicy::Strict;
    CHECK_THROWS(UnsupportedFeature,escape(profile,Trait::Bold));
    encodes("",profile,Traits{});
}
TEST(CAP_01_color_quantization_uses_specified_palette_and_ties)
{
    auto profile=contract::full_profile();
    profile.colors=ColorSupport::Basic8;
    encodes("\x1b[31m",profile,fg(TColor{0x800000}));
    encodes("\x1b[30m",profile,fg(TColor{0x400000})); // Equidistant black/dark red, lower slot wins.
    encodes("\x1b[41m",profile,bg(TColor{0x800000}));
    encodes("\x1b[31m",profile,fg(XColor::BrightRed)); // Fold brightness, never add Bold.
    encodes("\x1b[37m",profile,fg(XColor{255}));
    profile.colors=ColorSupport::Basic16;
    encodes("\x1b[91m",profile,fg(TColor{0xFF0000}));
    profile.system_palette[4]=RGB{1,2,3};
    encodes("\x1b[34m",profile,fg(TColor{0x010203}));
}
TEST(CAP_01_indexed_quantization_has_fixed_expected_candidates)
{
    auto profile=contract::full_profile();
    profile.colors=ColorSupport::Indexed256;
    encodes("\x1b[38;5;67m",profile,fg(TColor{0x5F87AF}));
    encodes("\x1b[38;5;232m",profile,fg(TColor{0x080808}));
    encodes("\x1b[38;5;245m",profile,fg(TColor{0x8A8A8A}));
    encodes("\x1b[31m",profile,fg(XColor::Red));
    encodes("\x1b[38;5;196m",profile,fg(XColor{196})); // Preserve slot even if RGB matches slot 9.
}
TEST(CAP_01_default_colors_remain_default_and_none_drops_only_colors)
{
    auto profile=contract::full_profile();
    profile.colors=ColorSupport::Basic8;
    encodes("\x1b[39m\x1b[49m",profile,fg(TermColor::Default),bg(TermColor::Default));
    profile.colors=ColorSupport::None;
    encodes("\x1b[22;23;24;25;27;28;29;1m",profile,fg(XColor::Red),Trait::Bold);
    encodes("",profile,fg(TermColor::Default));
    profile.fallback=FallbackPolicy::Strict;
    CHECK_THROWS(UnsupportedFeature,escape(profile,bg(XColor::Red)));
}
TEST(CAP_01_strict_policy_never_approximates_color)
{
    auto profile=contract::full_profile();
    profile.colors=ColorSupport::Basic8;
    profile.fallback=FallbackPolicy::Strict;
    encodes("\x1b[31m",profile,fg(XColor::Red));
    CHECK_THROWS(UnsupportedFeature,escape(profile,fg(XColor::BrightRed)));
    CHECK_THROWS(UnsupportedFeature,escape(profile,fg(XColor{67})));
    CHECK_THROWS(UnsupportedFeature,escape(profile,fg(TColor{0x800000})));
}
TEST(CAP_01_mouse_protocol_can_fallback_but_tracking_cannot)
{
    auto profile=contract::full_profile();
    constexpr auto off="\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1005l\x1b[?1006l\x1b[?1015l\x1b[?1016l";
    encodes(std::string{off}+"\x1b[?1006h\x1b[?1000h",profile,MouseReporting{MouseMode::Basic,MouseProtocol::Legacy});
    profile.maximum_mouse_mode=MouseMode::Basic;
    CHECK_THROWS(UnsupportedFeature,escape(profile,MouseReporting{MouseMode::Drag,MouseProtocol::Sgr}));
    profile.fallback=FallbackPolicy::Strict;
    CHECK_THROWS(UnsupportedFeature,escape(profile,MouseReporting{MouseMode::Basic,MouseProtocol::Legacy}));
}
TEST(CAP_01_mode_support_is_required_under_both_policies)
{
    for (auto policy : {FallbackPolicy::BestEffort,FallbackPolicy::Strict}) {
        auto profile=contract::full_profile();
        profile.fallback=policy;
        profile.alternate_screen=false;
        profile.cursor_visibility=false;
        profile.focus_reporting=false;
        profile.bracketed_paste=false;
        CHECK_THROWS(UnsupportedFeature,escape(profile,ScreenBuffer::Alternate));
        CHECK_THROWS(UnsupportedFeature,escape(profile,CursorMode::Hide));
        CHECK_THROWS(UnsupportedFeature,escape(profile,FocusReporting::On));
        CHECK_THROWS(UnsupportedFeature,escape(profile,BracketedPaste::On));
    }
}
TEST(BUF_01_capability_failure_is_transactional_for_all_commands)
{
    auto profile=contract::full_profile();
    profile.fallback=FallbackPolicy::Strict;
    profile.colors=ColorSupport::Basic8;
    std::string output="prefix";
    CHECK_THROWS(UnsupportedFeature,append_escape(output,profile,Cursor{},fg(TColor{0x010203})));
    CHECK(output == "prefix");
    std::array<char,128> bytes;
    bytes.fill('#');
    auto before=bytes;
    CHECK_THROWS(UnsupportedFeature,escape_to(bytes,profile,Cursor{},fg(TColor{0x010203})));
    CHECK(bytes == before);
}
TEST(CAP_02_inconsistent_and_invalid_profiles_are_rejected)
{
    auto profile=contract::full_profile();
    profile.control=false;
    CHECK_THROWS(std::invalid_argument,escape(profile,ResetStyle{}));
    profile=contract::full_profile();
    profile.mouse_protocol=MouseProtocol::None;
    CHECK_THROWS(std::invalid_argument,escape(profile,Cursor{}));
    profile=contract::full_profile();
    profile.colors=static_cast<ColorSupport>(99);
    CHECK_THROWS(std::invalid_argument,escape(profile,Cursor{}));
}
TEST(CAP_02_unknown_dumb_empty_and_redirected_output_are_plain)
{
    for (auto term : {"","dumb","unknown-terminal","xterm-256color-extra","not-xterm"}) {
        auto report=resolve_profile(Environment{.term=term,.colorterm="truecolor"},true);
        CHECK(!report.profile.control);
        CHECK(report.profile.colors == ColorSupport::None);
        CHECK(report.profile.traits == Traits{});
    }
    auto redirected=resolve_profile(Environment{.term="xterm-256color",.colorterm="truecolor"},false);
    CHECK(!redirected.profile.control);
    CHECK(redirected.profile.colors == ColorSupport::None);
}
TEST(CAP_03_exact_resolution_table_and_noncolor_capabilities)
{
    struct Case { char const* term; ColorSupport colors; bool full; bool alternate; bool visibility; bool mouse; };
    Case const cases[] = {
        {"ansi",ColorSupport::Basic8,false,false,false,false},
        {"xterm",ColorSupport::Basic8,true,true,true,true},
        {"xterm-color",ColorSupport::Basic8,true,true,true,true},
        {"xterm-256color",ColorSupport::Indexed256,true,true,true,true},
        {"screen",ColorSupport::Basic8,false,true,true,false},
        {"tmux",ColorSupport::Basic8,false,true,true,false},
        {"screen-256color",ColorSupport::Indexed256,false,true,true,false},
        {"tmux-256color",ColorSupport::Indexed256,false,true,true,false},
        {"linux",ColorSupport::Basic16,false,false,true,false},
    };
    for (auto const& c : cases) {
        auto report=resolve_profile(Environment{.term=c.term},true);
        auto const& p=report.profile;
        CHECK(p.control && p.colors == c.colors);
        CHECK(p.traits == (c.full ? contract::full_traits() : contract::basic_traits()));
        CHECK(p.alternate_screen == c.alternate && p.cursor_visibility == c.visibility);
        CHECK(p.mouse_protocol == (c.mouse ? MouseProtocol::Sgr : MouseProtocol::None));
        CHECK(p.maximum_mouse_mode == (c.mouse ? MouseMode::Move : MouseMode::Off));
        CHECK(p.focus_reporting == c.full && p.bracketed_paste == c.full);
    }
}
TEST(CAP_03_environment_precedence_and_case_sensitive_matching)
{
    for (auto hint : {"truecolor","24bit"}) {
        auto report=resolve_profile(Environment{.term="xterm-256color",.colorterm=hint},true);
        CHECK(report.profile.colors == ColorSupport::TrueColor);
    }
    auto no_hint=resolve_profile(Environment{.term="xterm-256color"},true);
    CHECK(no_hint.profile.colors == ColorSupport::Indexed256);
    auto wrong_case=resolve_profile(Environment{.term="xterm-256color",.colorterm="TrueColor"},true);
    CHECK(wrong_case.profile.colors == ColorSupport::Indexed256);
    auto no_color=resolve_profile(Environment{.term="xterm-256color",.colorterm="truecolor",.no_color=true},true);
    CHECK(no_color.profile.colors == ColorSupport::None);
    CHECK(no_color.profile.control && no_color.profile.traits == contract::full_traits());
    CHECK(no_color.profile.mouse_protocol == MouseProtocol::Sgr);
}
TEST(CAP_03_reports_have_one_ordered_decision_per_feature)
{
    auto report=resolve_profile(Environment{.term="xterm-256color"},true);
    Feature const features[] = {Feature::Control,Feature::Colors,Feature::Traits,
        Feature::AlternateScreen,Feature::CursorVisibility,Feature::Mouse,Feature::Focus,Feature::BracketedPaste};
    CHECK(report.decisions.size() == std::size(features) + 7);
    for (std::size_t i=0;i<std::size(features);++i) {
        CHECK(report.decisions[i].feature == features[i]);
        CHECK(report.decisions[i].source == EvidenceSource::KnownTerminal);
        CHECK(!report.decisions[i].explanation.empty());
    }
}

TEST(ENC_01_raw_encoding_does_not_consult_capabilities)
{
    auto const before = escape(fg(TColor{0x010203}));
    static_cast<void>(resolve_profile(Environment{.term="dumb",.no_color=true},false));
    static_cast<void>(escape(Trait::Bold));
    CHECK(escape(fg(TColor{0x010203})) == before);
    CHECK(before == "\x1b[38;2;1;2;3m");
}


TEST(CAP_01_disabled_modes_still_require_the_feature)
{
    for (auto policy : {FallbackPolicy::BestEffort,FallbackPolicy::Strict}) {
        TerminalProfile plain;
        plain.fallback=policy;
        CHECK_THROWS(UnsupportedFeature,escape(plain,MoveCursor{}));
        CHECK_THROWS(UnsupportedFeature,escape(plain,ScreenBuffer::Normal));
        CHECK_THROWS(UnsupportedFeature,escape(plain,CursorMode::Show));
        CHECK_THROWS(UnsupportedFeature,escape(plain,MouseReporting{}));
        CHECK_THROWS(UnsupportedFeature,escape(plain,FocusReporting::Off));
        CHECK_THROWS(UnsupportedFeature,escape(plain,BracketedPaste::Off));
    }
}
TEST(CAP_01_mouse_off_does_not_select_an_encoding)
{
    auto profile=contract::full_profile();
    profile.fallback=FallbackPolicy::Strict;
    for (auto protocol : {MouseProtocol::None,MouseProtocol::Legacy,MouseProtocol::Sgr,MouseProtocol::Urxvt})
        encodes(escape(MouseReporting{}),profile,MouseReporting{MouseMode::Off,protocol});
}
TEST(CAP_01_invalid_commands_are_not_hidden_by_style_fallback)
{
    TerminalProfile plain;
    CHECK_THROWS(std::invalid_argument,escape(plain,Cursor{-1,0}));
    CHECK_THROWS(std::invalid_argument,escape(plain,BlankRow{static_cast<Erase>(99)}));
    CHECK_THROWS(std::invalid_argument,escape(plain,static_cast<Trait>(1)));
    CHECK_THROWS(std::invalid_argument,escape(plain,static_cast<FocusReporting>(99)));
}
TEST(CAP_01_empty_pack_validates_the_profile_and_preserves_output)
{
    auto profile=contract::full_profile();
    profile.fallback=static_cast<FallbackPolicy>(99);
    std::string output="prefix";
    CHECK_THROWS(std::invalid_argument,append_escape(output,profile));
    CHECK(output=="prefix");
    std::array<char,8> bytes;
    bytes.fill('x');
    CHECK_THROWS(std::invalid_argument,escape_to(bytes,profile));
    CHECK(std::all_of(bytes.begin(),bytes.end(),[](char value){return value=='x';}));
    CHECK_THROWS(std::invalid_argument,escape(profile));
    encodes("",TerminalProfile{});
}
TEST(CAP_01_supported_indexed_colors_are_preserved_exhaustively)
{
    auto profile=contract::full_profile();
    for (auto depth : {ColorSupport::Indexed256,ColorSupport::TrueColor}) {
        profile.colors=depth;
        for (unsigned i=0;i<256;++i) {
            auto color=XColor{static_cast<std::uint8_t>(i)};
            encodes(escape(fg(color)),profile,fg(color));
        }
    }
    profile.colors=ColorSupport::Basic8;
    for (unsigned i=8;i<16;++i)
        encodes(escape(fg(XColor{static_cast<std::uint8_t>(i-8)})),profile,
                fg(XColor{static_cast<std::uint8_t>(i)}));
}
TEST(CAP_01_unsupported_errors_identify_the_feature)
{
    auto check=[](auto command,Feature expected) {
        try { (void)escape(TerminalProfile{},command); CHECK(false); }
        catch (UnsupportedFeature const& error) { CHECK(error.feature()==expected); }
    };
    check(Cursor{},Feature::Control);
    check(ScreenBuffer::Alternate,Feature::AlternateScreen);
    check(CursorMode::Hide,Feature::CursorVisibility);
    check(MouseReporting{},Feature::Mouse);
    check(FocusReporting::On,Feature::Focus);
    check(BracketedPaste::On,Feature::BracketedPaste);
}

#if defined(__unix__) || defined(__APPLE__)
#include "support/posix.hpp"
TEST(CAP_02_environment_snapshot_owns_values_and_resolution_is_pure)
{
    contract::EnvironmentVariable term{"TERM","xterm-256color"};
    contract::EnvironmentVariable colorterm{"COLORTERM","truecolor"};
    contract::EnvironmentVariable no_color{"NO_COLOR",""};
    auto snapshot=snapshot_environment();
    CHECK(snapshot.term=="xterm-256color" && snapshot.colorterm=="truecolor" && !snapshot.no_color);
    {
        contract::EnvironmentVariable changed_term{"TERM","dumb"};
        contract::EnvironmentVariable changed_color{"NO_COLOR","1"};
        CHECK(snapshot_environment().no_color);
        CHECK(snapshot.term=="xterm-256color");
        CHECK(resolve_profile(snapshot,true).profile.colors==ColorSupport::TrueColor);
        CHECK(resolve_profile(Environment{"ansi","",false},true).profile.colors==ColorSupport::Basic8);
    }
    contract::EnvironmentVariable absent{"NO_COLOR",nullptr};
    CHECK(!snapshot_environment().no_color);
}
#endif
