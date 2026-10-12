#include "support/fixtures.hpp"
#include "support/results.hpp"

#include <climits>

using namespace esc;
using namespace std::literals;
using contract::parses_at_every_split;

TEST(PAR_01_ascii_unicode_and_control_keys_do_not_invent_modifiers)
{
    parses_at_every_split(
        "aA界👽\0\x01\n\t\r\b\x7f"sv,
        {KeyEvent{U'a'}, KeyEvent{U'A'}, KeyEvent{U'界'}, KeyEvent{U'👽'},
         KeyEvent{U'\0'}, KeyEvent{U'\x01'}, KeyEvent{U'\n'}, KeyEvent{KeyCode::Tab},
         KeyEvent{KeyCode::Enter}, KeyEvent{KeyCode::Backspace},
         KeyEvent{KeyCode::Backspace}});
}

TEST(PAR_01_csi_and_ss3_navigation_and_function_keys)
{
    struct Case {
        char final;
        KeyCode key;
    };

    Case const cases[] = {{'A', KeyCode::ArrowUp},    {'B', KeyCode::ArrowDown},
                          {'C', KeyCode::ArrowRight}, {'D', KeyCode::ArrowLeft},
                          {'H', KeyCode::Home},       {'F', KeyCode::End},
                          {'P', KeyCode::F1},         {'Q', KeyCode::F2},
                          {'R', KeyCode::F3},         {'S', KeyCode::F4}};
    for (auto c : cases) {
        for (auto prefix : {"\x1b[", "\x1b[1", "\x1bO"}) {
            auto bytes = std::string{prefix} + c.final;
            parses_at_every_split(bytes, {KeyEvent{c.key}});
        }
    }
    parses_at_every_split("\x1b[Z", {KeyEvent{KeyCode::Tab, Modifiers{.shift = true}}});
}

TEST(PAR_01_tilde_identifiers_use_named_keys_not_enum_arithmetic)
{
    struct Case {
        int identifier;
        KeyCode key;
    };

    Case const cases[] = {
        {1, KeyCode::Home},   {7, KeyCode::Home},     {2, KeyCode::Insert},
        {3, KeyCode::Delete}, {4, KeyCode::End},      {8, KeyCode::End},
        {5, KeyCode::PageUp}, {6, KeyCode::PageDown}, {11, KeyCode::F1},
        {12, KeyCode::F2},    {13, KeyCode::F3},      {14, KeyCode::F4},
        {15, KeyCode::F5},    {17, KeyCode::F6},      {18, KeyCode::F7},
        {19, KeyCode::F8},    {20, KeyCode::F9},      {21, KeyCode::F10},
        {23, KeyCode::F11},   {24, KeyCode::F12},     {25, KeyCode::F13},
        {26, KeyCode::F14},   {28, KeyCode::F15},     {29, KeyCode::F16},
        {31, KeyCode::F17},   {32, KeyCode::F18},     {33, KeyCode::F19},
        {34, KeyCode::F20}};
    for (auto c : cases)
        parses_at_every_split("\x1b[" + std::to_string(c.identifier) + "~",
                              {KeyEvent{c.key}});
}

TEST(PAR_01_ss3_keypad_is_distinct_from_printable_text)
{
    struct Case {
        char final;
        KeyCode key;
    };

    Case const cases[] = {{'M', KeyCode::KeypadEnter},    {'p', KeyCode::Keypad0},
                          {'q', KeyCode::Keypad1},        {'r', KeyCode::Keypad2},
                          {'s', KeyCode::Keypad3},        {'t', KeyCode::Keypad4},
                          {'u', KeyCode::Keypad5},        {'v', KeyCode::Keypad6},
                          {'w', KeyCode::Keypad7},        {'x', KeyCode::Keypad8},
                          {'y', KeyCode::Keypad9},        {'n', KeyCode::KeypadDecimal},
                          {'k', KeyCode::KeypadPlus},     {'m', KeyCode::KeypadMinus},
                          {'j', KeyCode::KeypadMultiply}, {'o', KeyCode::KeypadDivide}};
    for (auto c : cases)
        parses_at_every_split(std::string{"\x1bO"} + c.final, {KeyEvent{c.key}});
    parses_at_every_split("0", {KeyEvent{U'0'}});
}

TEST(PAR_01_native_function_identifiers_are_not_portable_protocol_claims)
{
    for (auto identifier : {42, 43, 44, 45}) {
        auto bytes = "\x1b[" + std::to_string(identifier) + "~";
        parses_at_every_split(bytes,
                              {UnknownInput{bytes, InputProblem::Unsupported, true}});
    }
}

TEST(PAR_01_every_modifier_combination_is_a_separate_value)
{
    for (int parameter = 1; parameter <= 16; ++parameter) {
        auto bits = parameter - 1;
        Modifiers modifiers{bool(bits & 1), bool(bits & 4), bool(bits & 2),
                            bool(bits & 8)};
        parses_at_every_split("\x1b[1;" + std::to_string(parameter) + "A",
                              {KeyEvent{KeyCode::ArrowUp, modifiers}});
        parses_at_every_split("\x1b[11;" + std::to_string(parameter) + "~",
                              {KeyEvent{KeyCode::F1, modifiers}});
    }
}

TEST(PAR_01_sgr_mouse_press_release_and_motion)
{
    InputProtocols protocol{.mouse = MouseProtocol::Sgr};
    parses_at_every_split("\x1b[<0;5;7M", {MousePress{{4, 6}, MouseButton::Left}},
                          protocol);
    parses_at_every_split("\x1b[<1;5;7M", {MousePress{{4, 6}, MouseButton::Middle}},
                          protocol);
    parses_at_every_split("\x1b[<2;5;7M", {MousePress{{4, 6}, MouseButton::Right}},
                          protocol);
    parses_at_every_split("\x1b[<0;5;7m", {MouseRelease{{4, 6}, MouseButton::Left}},
                          protocol);
    parses_at_every_split("\x1b[<3;5;7m", {MouseRelease{{4, 6}, std::nullopt}},
                          protocol);
    parses_at_every_split("\x1b[<32;5;7M", {MouseMove{{4, 6}, MouseButton::Left}},
                          protocol);
    parses_at_every_split("\x1b[<35;5;7M", {MouseMove{{4, 6}, std::nullopt}}, protocol);
    parses_at_every_split(
        "\x1b[<52;5;7M",
        {MouseMove{{4, 6}, MouseButton::Left, Modifiers{.shift = true, .ctrl = true}}},
        protocol);
}

TEST(PAR_01_wheels_are_deltas_not_extra_mouse_buttons)
{
    InputProtocols protocol{.mouse = MouseProtocol::Sgr};
    parses_at_every_split("\x1b[<64;1;1M", {MouseWheel{{0, 0}, 0, -1}}, protocol);
    parses_at_every_split("\x1b[<65;1;1M", {MouseWheel{{0, 0}, 0, 1}}, protocol);
    parses_at_every_split("\x1b[<66;1;1M", {MouseWheel{{0, 0}, -1, 0}}, protocol);
    parses_at_every_split("\x1b[<67;1;1M", {MouseWheel{{0, 0}, 1, 0}}, protocol);
    parses_at_every_split(
        "\x1b[<92;1;1M",
        {MouseWheel{
            {0, 0}, 0, -1, Modifiers{.shift = true, .ctrl = true, .alt = true}}},
        protocol);
    for (auto [code, button] :
         {std::pair{128, MouseButton::Button8}, std::pair{129, MouseButton::Button9},
          std::pair{130, MouseButton::Button10}, std::pair{131, MouseButton::Button11}})
        parses_at_every_split("\x1b[<" + std::to_string(code) + ";1;1M",
                              {MousePress{{0, 0}, button}}, protocol);
}

TEST(PAR_01_legacy_release_does_not_guess_previous_button)
{
    // Each byte encodes its field plus 32; positions are wire column 5,row 7.
    parses_at_every_split(
        "\x1b[M\x20\x25\x27\x1b[M\x23\x25\x27"sv,
        {MousePress{{4, 6}, MouseButton::Left}, MouseRelease{{4, 6}, std::nullopt}},
        InputProtocols{.mouse = MouseProtocol::Legacy});
    parses_at_every_split(
        "\x1b[32;5;7M\x1b[35;5;7M",
        {MousePress{{4, 6}, MouseButton::Left}, MouseRelease{{4, 6}, std::nullopt}},
        InputProtocols{.mouse = MouseProtocol::Urxvt});
}

TEST(PAR_01_mouse_boundaries_use_cell_coordinates_without_overflow)
{
    InputProtocols protocol{.mouse = MouseProtocol::Sgr};
    parses_at_every_split("\x1b[<0;2147483648;2147483648M",
                          {MousePress{{INT_MAX, INT_MAX}, MouseButton::Left}},
                          protocol);
    for (auto bytes :
         {"\x1b[<0;0;1M", "\x1b[<0;1;0M", "\x1b[<0;2147483649;1M", "\x1b[<3;1;1M",
          "\x1b[<96;1;1M", "\x1b[<64;1;1m", "\x1b[<256;1;1M"}) {
        auto events = contract::parse(bytes, protocol);
        CHECK(contract::unknown_bytes(events) == bytes);
    }
}

TEST(PAR_04_focus_and_mouse_protocols_are_not_enabled_implicitly)
{
    parses_at_every_split("\x1b[I\x1b[O", {Focus{true}, Focus{false}},
                          InputProtocols{.focus = true});
    for (auto bytes : {"\x1b[I", "\x1b[O", "\x1b[<0;1;1M", "\x1b[200~"}) {
        auto events = contract::parse(bytes);
        CHECK(contract::unknown_bytes(events) == bytes);
    }
}

TEST(PAR_02_mixed_event_stream_survives_every_fragment_boundary)
{
    parses_at_every_split("a界\x1b[1;5A\x1b[<0;2;3M\x1b[I",
                          {KeyEvent{U'a'}, KeyEvent{U'界'},
                           KeyEvent{KeyCode::ArrowUp, Modifiers{.ctrl = true}},
                           MousePress{{1, 2}, MouseButton::Left}, Focus{true}},
                          InputProtocols{.mouse = MouseProtocol::Sgr, .focus = true});
}

TEST(PAR_03_standalone_escape_and_alt_text_have_explicit_expiry)
{
    Parser parser;
    std::vector<Event> events;
    contract::feed(parser, "\x1b", events);
    CHECK(events.empty());
    CHECK(parser.pending_timeout() == std::optional{PendingTimeout::Escape});
    CHECK(parser.expire() == std::optional<Event>{KeyEvent{KeyCode::Escape}});
    CHECK(!parser.expire());
    CHECK(!parser.pending_timeout());
    parses_at_every_split(
        "\x1b"
        "a\x1b界",
        {KeyEvent{U'a', Modifiers{.alt = true}},
         KeyEvent{U'界', Modifiers{.alt = true}}});
}

TEST(PAR_03_double_escape_resolves_first_and_keeps_second_pending)
{
    Parser parser;
    std::vector<Event> events;
    contract::feed(parser, "\x1b\x1b", events);
    CHECK(events == std::vector<Event>{KeyEvent{KeyCode::Escape}});
    CHECK(parser.expire() == std::optional<Event>{KeyEvent{KeyCode::Escape}});
}

TEST(PAR_03_truncated_sequences_expire_without_inventing_keys)
{
    for (auto bytes : {"\x1b[", "\x1b[1;", "\x1bO", "\x1b]title", "\x1b\xc2"}) {
        Parser parser;
        std::vector<Event> events;
        contract::feed(parser, bytes, events);
        CHECK(events.empty());
        CHECK(parser.pending_timeout() == std::optional{PendingTimeout::Sequence});
        auto expired = parser.expire();
        CHECK(expired.has_value());
        CHECK(std::get<UnknownInput>(*expired).bytes == bytes);
        CHECK(std::get<UnknownInput>(*expired).problem == InputProblem::Truncated);
        contract::feed(parser, "x", events);
        CHECK(events == std::vector<Event>{KeyEvent{U'x'}});
    }
}

TEST(PAR_03_plain_partial_utf8_has_no_clock_deadline)
{
    Parser parser;
    std::vector<Event> events;
    contract::feed(parser, "\xe7\x95"sv, events);
    CHECK(events.empty() && !parser.pending_timeout());
    CHECK(!parser.expire());
    contract::feed(parser, "\x8c"sv, events);
    CHECK(events == std::vector<Event>{KeyEvent{U'界'}});
}

TEST(PAR_03_finish_drains_truncation_then_requires_reset)
{
    Parser parser;
    std::vector<Event> events;
    contract::feed(parser, "\xe7\x95"sv, events);
    auto last = parser.finish();
    CHECK(last.has_value());
    CHECK(std::get<UnknownInput>(*last).bytes == "\xe7\x95"sv);
    CHECK(std::get<UnknownInput>(*last).problem == InputProblem::Truncated);
    CHECK(!parser.finish() && !parser.finish());
    CHECK_THROWS(std::logic_error, parser.next(std::span<char const>{}));
    parser.reset();
    contract::feed(parser, "x", events);
    CHECK(events == std::vector<Event>{KeyEvent{U'x'}});
}

TEST(PAR_04_unknown_control_units_preserve_payload)
{
    for (auto bytes :
         {"\x1b[999~", "\x1b[12;40R", "\x1b]payload\a", "\x1bPpayload\x1b\\",
          "\x1b_payload\x1b\\", "\x1b^payload\x1b\\"}) {
        auto expected =
            std::vector<Event>{UnknownInput{bytes, InputProblem::Unsupported, true}};
        parses_at_every_split(bytes, expected);
    }
}

TEST(PAR_04_malformed_parameters_do_not_become_partial_numbers)
{
    for (auto bytes : {"\x1b[1;0A", "\x1b[1;17A", "\x1b[1;;5A", "\x1b[1;5;2A",
                       "\x1b[1:5A", "\x1b[999999999999999999999999~"}) {
        auto events = contract::parse(bytes);
        CHECK(contract::unknown_bytes(events) == bytes);
        CHECK(events.size() == 1);
    }
    parses_at_every_split("\x1b[01;05A",
                          {KeyEvent{KeyCode::ArrowUp, Modifiers{.ctrl = true}}});
}

TEST(PAR_04_invalid_utf8_preserves_bad_bytes_and_following_valid_text)
{
    for (auto bad : {"\x80"sv, "\xc0\xaf"sv, "\xed\xa0\x80"sv, "\xf4\x90\x80\x80"sv}) {
        auto events = contract::parse(std::string{bad} + "x");
        CHECK(!events.empty() && events.back() == Event{KeyEvent{U'x'}});
        events.pop_back();
        CHECK(contract::unknown_bytes(events) == bad);
        for (auto const& event : events)
            CHECK(std::get<UnknownInput>(event).problem == InputProblem::InvalidUtf8);
    }
}

TEST(PAR_04_overlong_opaque_units_remain_bounded_and_do_not_leak_as_keys)
{
    std::string bytes = "\x1b]" + std::string(10000, 'x') + "\x1b\\";
    auto events = contract::parse(bytes, {}, ParserOptions{.sequence_limit = 16});
    CHECK(events.size() > 1);
    CHECK(contract::unknown_bytes(events) == bytes);
    for (std::size_t i = 0; i < events.size(); ++i) {
        auto const& event = std::get<UnknownInput>(events[i]);
        CHECK(event.bytes.size() <= 16);
        CHECK(event.problem == InputProblem::TooLong);
        CHECK(event.final == (i + 1 == events.size()));
    }
}

TEST(PAR_04_configuration_changes_only_new_sequence_beginnings)
{
    Parser parser;
    std::vector<Event> events;
    contract::feed(parser, "\x1b[", events);
    parser.configure(InputProtocols{.focus = true});
    contract::feed(parser, "I\x1b[I", events);
    CHECK(events ==
          std::vector<Event>{UnknownInput{"\x1b[I", InputProblem::Unsupported, true},
                             Focus{true}});
    contract::feed(parser, "\x1b", events);
    parser.reset();
    events.clear();
    contract::feed(parser, "\x1b[I", events);
    CHECK(events == std::vector<Event>{Focus{true}});
}

TEST(PAR_04_cancellation_and_new_introducers_recover_in_order)
{
    parses_at_every_split(
        "\x1b[1\x18"
        "a",
        {UnknownInput{"\x1b[1\x18", InputProblem::Malformed, true}, KeyEvent{U'a'}});
    parses_at_every_split("\x1b[1\x1b[A",
                          {UnknownInput{"\x1b[1", InputProblem::Truncated, true},
                           KeyEvent{KeyCode::ArrowUp}});
}

TEST(PAR_05_paste_preserves_bytes_across_every_split)
{
    auto const input = "\x1b[200~a\0界\x1b[A\x1b[201~"sv;
    for (std::size_t split = 0; split <= input.size(); ++split) {
        Parser parser{{}, {.bracketed_paste = true}};
        std::vector<Event> events;
        contract::feed(parser, input.substr(0, split), events);
        contract::feed(parser, input.substr(split), events);
        CHECK(!events.empty());
        std::string joined;
        for (std::size_t i = 0; i < events.size(); ++i) {
            auto const& chunk = std::get<PasteChunk>(events[i]);
            CHECK(chunk.first == (i == 0) && chunk.last == (i + 1 == events.size()));
            CHECK(!chunk.aborted);
            joined += chunk.bytes;
        }
        CHECK(joined ==
              "a\0界\x1b[A"sv);  // Assert payload, not arbitrary chunk boundaries.
    }
}

TEST(PAR_05_empty_paste_is_a_single_first_and_last_chunk)
{
    parses_at_every_split("\x1b[200~\x1b[201~", {PasteChunk{"", true, true, false}},
                          InputProtocols{.bracketed_paste = true});
}

TEST(PAR_05_large_paste_streams_bounded_chunks)
{
    std::string payload = std::string(10000, 'a') + "界";
    auto events = contract::parse("\x1b[200~" + payload + "\x1b[201~",
                                  InputProtocols{.bracketed_paste = true},
                                  ParserOptions{.paste_chunk_size = 4});
    std::string joined;
    for (std::size_t i = 0; i < events.size(); ++i) {
        auto const& chunk = std::get<PasteChunk>(events[i]);
        CHECK(chunk.bytes.size() <= 4);
        CHECK(chunk.first == (i == 0));
        CHECK(chunk.last == (i + 1 == events.size()));
        CHECK(!chunk.aborted);
        joined += chunk.bytes;
    }
    CHECK(joined == payload);
}

TEST(PAR_05_eof_aborts_paste_without_losing_payload)
{
    Parser parser{{.paste_chunk_size = 4}, {.bracketed_paste = true}};
    std::vector<Event> events;
    contract::feed(parser, "\x1b[200~abcdef", events);
    while (auto event = parser.finish())
        events.push_back(std::move(*event));
    std::string joined;
    CHECK(!events.empty());
    for (auto const& event : events)
        joined += std::get<PasteChunk>(event).bytes;
    CHECK(joined == "abcdef");
    auto const& last = std::get<PasteChunk>(events.back());
    CHECK(last.last && last.aborted);
    CHECK(std::get<PasteChunk>(events.front()).first);
}

TEST(PAR_01_constructor_rejects_invalid_limits_and_protocols)
{
    CHECK_THROWS(std::invalid_argument, Parser{ParserOptions{.sequence_limit = 15}});
    CHECK_THROWS(std::invalid_argument, Parser{ParserOptions{.sequence_limit = 4097}});
    CHECK_THROWS(std::invalid_argument, Parser{ParserOptions{.paste_chunk_size = 0}});
    CHECK_THROWS(std::invalid_argument,
                 Parser{ParserOptions{.paste_chunk_size = 4097}});
    CHECK_THROWS(
        std::invalid_argument,
        Parser{ParserOptions{.escape_timeout = std::chrono::milliseconds{-1}}});
    CHECK_THROWS(
        std::invalid_argument,
        Parser{ParserOptions{.sequence_timeout = std::chrono::milliseconds{-1}}});
    CHECK_THROWS(std::invalid_argument,
                 Parser{{}, InputProtocols{.mouse = static_cast<MouseProtocol>(99)}});
}

TEST(PAR_03_finish_resolves_standalone_escape_as_a_key)
{
    Parser parser;
    std::vector<Event> events;
    contract::feed(parser, "\x1b", events);
    CHECK(parser.finish() == std::optional<Event>{KeyEvent{KeyCode::Escape}});
    CHECK(!parser.finish());
}

TEST(PAR_04_all_ss3_and_opaque_truncations_remain_owned_after_input_dies)
{
    Parser parser;
    {
        std::string input = "\x1b]owned-payload";
        auto result = parser.next(std::span<char const>{input.data(), input.size()});
        CHECK(result.consumed == input.size() && !contract::event(result));
    }
    auto event = parser.finish();
    CHECK(std::get<UnknownInput>(*event).bytes == "\x1b]owned-payload");
}

TEST(PAR_04_overlong_input_is_fragmentation_independent_in_payload_and_finality)
{
    auto input = "\x1b]" + std::string(100, 'x') + "\x1b\\";
    Parser parser{{.sequence_limit = 16}};
    std::vector<Event> events;
    for (std::size_t i = 0; i < input.size(); ++i)
        contract::feed(parser, std::string_view{input}.substr(i, 1), events);
    CHECK(contract::unknown_bytes(events) == input);
    CHECK(std::get<UnknownInput>(events.back()).final);
    for (std::size_t i = 0; i + 1 < events.size(); ++i)
        CHECK(!std::get<UnknownInput>(events[i]).final);
}

TEST(PAR_01_text_scalar_does_not_guess_named_key_text)
{
    CHECK(text_scalar(Key{U'\0'}) == std::optional<char32_t>{U'\0'});
    CHECK(text_scalar(Key{U'界'}) == std::optional<char32_t>{U'界'});
    CHECK(!text_scalar(Key{KeyCode::Enter}).has_value());
    CHECK(!text_scalar(Key{KeyCode::F1}).has_value());
}

TEST(PAR_04_already_invalid_unicode_prefix_does_not_wait_for_eof)
{
    for (auto bytes : {"\xe0\x80"sv, "\xed\xa0"sv, "\xf0\x80"sv, "\xf4\x90"sv}) {
        Parser parser;
        auto result = parser.next(std::span<char const>{bytes.data(), bytes.size()});
        CHECK(contract::status(result) == contract::Status::Event);
        CHECK(std::get<UnknownInput>(*contract::event(result)).problem ==
              InputProblem::InvalidUtf8);
        CHECK(std::get<UnknownInput>(*contract::event(result)).bytes == bytes);
        CHECK(!parser.finish());
    }
}

TEST(PAR_05_partial_and_false_paste_delimiters_preserve_every_byte)
{
    auto payload = "a\x1b[20x\x1b[201X\x1b\x1b[201"sv;
    auto input = std::string{"\x1b[200~"} + std::string{payload} + "\x1b[201~";
    for (std::size_t chunk = 1; chunk <= 8; ++chunk) {
        Parser parser{{.paste_chunk_size = chunk}, {.bracketed_paste = true}};
        std::vector<Event> events;
        for (auto byte : input)
            contract::feed(parser, std::string_view{&byte, 1}, events);
        std::string joined;
        for (std::size_t i = 0; i < events.size(); ++i) {
            auto const& event = std::get<PasteChunk>(events[i]);
            CHECK(event.bytes.size() <= chunk);
            CHECK(event.first == (i == 0) && event.last == (i + 1 == events.size()));
            joined += event.bytes;
        }
        CHECK(joined == payload);
    }
}

TEST(PAR_05_bulk_paste_preserves_payload_and_following_input)
{
    std::string payload(257, 'p');
    payload += "\x1b[20x\x1b[201X\x1b\x1b[201";
    payload += std::string(257, '\0');
    auto input = "\x1b[200~" + payload + "\x1b[201~z";
    for (auto chunk : {1u, 4u, 64u, 256u, 4096u}) {
        for (auto fragment : {1u, 5u, 6u, 63u, 64u, 257u, 1024u}) {
            Parser parser{{.paste_chunk_size = chunk}, {.bracketed_paste = true}};
            std::vector<Event> events;
            for (std::size_t offset = 0; offset < input.size(); offset += fragment)
                contract::feed(parser, std::string_view{input}.substr(offset, fragment),
                               events);
            CHECK(events.back() == Event{KeyEvent{U'z'}});
            events.pop_back();
            std::string joined;
            for (std::size_t i = 0; i < events.size(); ++i) {
                auto const& paste = std::get<PasteChunk>(events[i]);
                CHECK(paste.bytes.size() <= chunk);
                CHECK(paste.first == (i == 0));
                CHECK(paste.last == (i + 1 == events.size()));
                CHECK(!paste.aborted);
                joined += paste.bytes;
            }
            CHECK(joined == payload);
            CHECK(!parser.finish());
        }
    }
}

TEST(PAR_04_overflow_then_expiry_or_eof_closes_the_same_opaque_unit)
{
    for (bool finish : {false, true}) {
        Parser parser{{.sequence_limit = 16}};
        std::vector<Event> events;
        std::string input = "\x1b]" + std::string(100, 'a');
        contract::feed(parser, input, events);
        auto final = finish ? parser.finish() : parser.expire();
        CHECK(final.has_value());
        events.push_back(*final);
        CHECK(contract::unknown_bytes(events) == input);
        for (std::size_t i = 0; i < events.size(); ++i) {
            auto const& unknown = std::get<UnknownInput>(events[i]);
            CHECK(unknown.bytes.size() <= 16 &&
                  unknown.problem == InputProblem::TooLong);
            CHECK(unknown.final == (i + 1 == events.size()));
        }
    }
}

TEST(PAR_04_reset_and_configuration_validation_preserve_contract)
{
    Parser parser{{}, {.focus = true}};
    CHECK_THROWS(
        std::invalid_argument,
        parser.configure(InputProtocols{.mouse = static_cast<MouseProtocol>(99)}));
    CHECK(contract::parse("\x1b[I", {.focus = true}) ==
          std::vector<Event>{Focus{true}});
    std::vector<Event> events;
    contract::feed(parser, "\x1b[I", events);
    CHECK(events == std::vector<Event>{Focus{true}});
    parser.finish();
    parser.reset();
    events.clear();
    contract::feed(parser, "\x1b[I", events);
    CHECK(events == std::vector<Event>{Focus{true}});
}

TEST(PAR_02_deterministic_mixed_streams_match_independent_event_goldens)
{
    struct Fixture {
        std::string_view bytes;
        Event event;
    };

    std::array fixtures = {
        Fixture{"a", KeyEvent{U'a'}},
        Fixture{"界", KeyEvent{U'界'}},
        Fixture{"\x1b[1;5A", KeyEvent{KeyCode::ArrowUp, Modifiers{.ctrl = true}}},
        Fixture{"\x1bOP", KeyEvent{KeyCode::F1}},
        Fixture{"\x1b[I", Focus{true}},
        Fixture{"\x1b[O", Focus{false}},
        Fixture{"\x1b[<0;12;8M", MousePress{{11, 7}, MouseButton::Left}},
        Fixture{"\x1b[<0;12;8m", MouseRelease{{11, 7}, MouseButton::Left}},
        Fixture{"\x1b[<65;1;1M", MouseWheel{{0, 0}, 0, 1}},
        Fixture{"\x1b]reply\a",
                UnknownInput{"\x1b]reply\a", InputProblem::Unsupported, true}},
        Fixture{"\xff", UnknownInput{"\xff", InputProblem::InvalidUtf8, true}},
        Fixture{"\x1b\t", KeyEvent{KeyCode::Tab, Modifiers{.alt = true}}}};
    std::uint32_t seed = 0x12345678;
    auto random = [&] {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };
    for (int trial = 0; trial < 40; ++trial) {
        std::string input;
        std::vector<Event> expected;
        for (int i = 0; i < 100; ++i) {
            auto const& fixture = fixtures[random() % fixtures.size()];
            input += fixture.bytes;
            expected.push_back(fixture.event);
        }
        Parser parser{{}, {.mouse = MouseProtocol::Sgr, .focus = true}};
        std::vector<Event> actual;
        for (std::size_t offset = 0; offset < input.size();) {
            auto size = std::min<std::size_t>(1 + random() % 13, input.size() - offset);
            contract::feed(parser, std::string_view{input}.substr(offset, size),
                           actual);
            offset += size;
        }
        CHECK(actual == expected);
        CHECK(!parser.finish());
    }
}
