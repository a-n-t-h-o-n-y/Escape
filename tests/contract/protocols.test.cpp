#include "support/allocations.hpp"
#include "support/fixtures.hpp"
using namespace esc;
using namespace std::literals;

TEST(PAR_QUERY_requests_have_literal_bytes_and_bounded_output)
{
    CHECK(encode_query(DeviceAttributesQuery{}) == "\x1b[c");
    CHECK(encode_query(KeyboardQuery{}) == "\x1b[?u");
    CHECK(encode_query(ModeQuery{2004}) == "\x1b[?2004$p");
    std::array<char, 4> bytes{'x', 'x', 'x', 'x'};
    auto result = query_to(bytes, ModeQuery{2004});
    CHECK(!result.written && result.required == 9);
    CHECK(bytes == std::array<char, 4>{'x', 'x', 'x', 'x'});
    CHECK_THROWS(std::invalid_argument, query_to(bytes, ModeQuery{0}));
    CHECK_THROWS(std::invalid_argument, encode_query(ModeQuery{65536}));
}

TEST(PAR_QUERY_replies_survive_every_fragment_boundary)
{
    InputProtocols p{.replies = true};
    contract::parses_at_every_split(
        "\x1b[?1;2;4c", {TerminalReply{DeviceAttributesReply{{1, 2, 4}}}}, p);
    contract::parses_at_every_split("\x1b[?31u", {TerminalReply{KeyboardReply{31}}}, p);
    for (unsigned status = 0; status <= 4; ++status)
        contract::parses_at_every_split(
            "\x1b[?2004;" + std::to_string(status) + "$y",
            {TerminalReply{ModeReply{2004, static_cast<ModeStatus>(status)}}}, p);
    contract::parses_at_every_split("a\x1b[?7u\x1b[Ab",
                                    {KeyEvent{U'a'}, TerminalReply{KeyboardReply{7}},
                                     KeyEvent{KeyCode::ArrowUp}, KeyEvent{U'b'}},
                                    p);
}

TEST(PAR_QUERY_reply_parsing_is_explicit_and_does_not_confuse_F3)
{
    contract::parses_at_every_split(
        "\x1b[?7u", {UnknownInput{"\x1b[?7u", InputProblem::Malformed, true}});
    contract::parses_at_every_split(
        "\x1b[1;2R", {KeyEvent{KeyCode::F3, {.shift = true}}}, {.replies = true});
}

TEST(PAR_QUERY_invalid_replies_remain_owned_unknown_units)
{
    for (auto bytes : {"\x1b[?u", "\x1b[?0;1$y", "\x1b[?25;5$y", "\x1b[?25;1;2$y",
                       "\x1b[?1;c", "\x1b[?42949672960u"})
        contract::parses_at_every_split(
            bytes, {UnknownInput{bytes, InputProblem::Malformed, true}},
            {.replies = true});
}

TEST(PAR_KITTY_identity_modifiers_actions_and_text_are_separate)
{
    InputProtocols p{.keyboard = KeyboardProtocol::Kitty};
    KeyEvent key;
    key.key = Key{U'a'};
    key.modifiers.shift = true;
    key.shifted_key = U'A';
    key.base_layout_key = U'a';
    key.text = U"A界";
    contract::parses_at_every_split("\x1b[97:65:97;2;65:30028u", {key}, p);
    key = {};
    key.key = Key{U'a'};
    key.action = KeyAction::Repeat;
    contract::parses_at_every_split("\x1b[97;1:2u", {key}, p);
    key.action = KeyAction::Release;
    contract::parses_at_every_split("\x1b[97;1:3u", {key}, p);
    key = {};
    key.text = U"å";
    contract::parses_at_every_split("\x1b[0;;229u", {key}, p);
    key = {};
    key.key = Key{U'a'};
    key.base_layout_key = U'c';
    contract::parses_at_every_split("\x1b[97::99u", {key}, p);
}

TEST(PAR_KITTY_every_modifier_bit_has_its_own_meaning)
{
    for (unsigned bits = 0; bits < 256; ++bits) {
        KeyEvent key;
        key.key = Key{U'a'};
        key.modifiers = {bool(bits & 1),  bool(bits & 4),  bool(bits & 2),
                         bool(bits & 32), bool(bits & 8),  bool(bits & 16),
                         bool(bits & 64), bool(bits & 128)};
        contract::parses_at_every_split("\x1b[97;" + std::to_string(bits + 1) + "u",
                                        {key}, {.keyboard = KeyboardProtocol::Kitty});
    }
}

TEST(PAR_KITTY_functional_and_legacy_forms_have_independent_goldens)
{
    struct Case {
        char const* bytes;
        KeyCode code;
    };

    Case cases[] = {{"\x1b[27u", KeyCode::Escape},
                    {"\x1b[13u", KeyCode::Enter},
                    {"\x1b[9u", KeyCode::Tab},
                    {"\x1b[127u", KeyCode::Backspace},
                    {"\x1b[57358u", KeyCode::CapsLock},
                    {"\x1b[57387u", KeyCode::F24},
                    {"\x1b[57398u", KeyCode::F35},
                    {"\x1b[57414u", KeyCode::KeypadEnter},
                    {"\x1b[57454u", KeyCode::IsoLevel5Shift},
                    {"\x1b[A", KeyCode::ArrowUp},
                    {"\x1b[13~", KeyCode::F3},
                    {"\x1b[1E", KeyCode::KeypadBegin}};
    for (auto const& c : cases) {
        KeyEvent key;
        key.key = Key{c.code};
        contract::parses_at_every_split(c.bytes, {key},
                                        {.keyboard = KeyboardProtocol::Kitty});
    }
    KeyEvent key;
    key.key = Key{KeyCode::ArrowUp};
    key.modifiers.ctrl = true;
    key.action = KeyAction::Release;
    contract::parses_at_every_split("\x1b[1;5:3A", {key},
                                    {.keyboard = KeyboardProtocol::Kitty});
    key = {};
    key.key = Key{KittyKey{60000}};
    contract::parses_at_every_split("\x1b[60000u", {key},
                                    {.keyboard = KeyboardProtocol::Kitty});
}

TEST(PAR_KITTY_invalid_fields_do_not_invent_text_or_keys)
{
    for (auto bytes :
         {"\x1b[u", "\x1b[0u", "\x1b[97;0u", "\x1b[97;257u", "\x1b[97;1:4u",
          "\x1b[55296u", "\x1b[1114112u", "\x1b[97:65u", "\x1b[97;;10u",
          "\x1b[97;;127u", "\x1b[97;;65:u", "\x1b[97;1;65;66u", "\x1b[1:2Au"}) {
        auto events = contract::parse(bytes, {.keyboard = KeyboardProtocol::Kitty});
        CHECK(std::holds_alternative<UnknownInput>(events.front()));
    }
    contract::parses_at_every_split(
        "\x1b[1;2R", {UnknownInput{"\x1b[1;2R", InputProblem::Unsupported, true}},
        {.keyboard = KeyboardProtocol::Kitty});
    CHECK_THROWS(std::invalid_argument,

                 Parser({}, {.keyboard = static_cast<KeyboardProtocol>(99)}));
}

TEST(PAR_KITTY_protocol_snapshot_and_plain_text_keep_their_contracts)
{
    Parser parser{{}, {.keyboard = KeyboardProtocol::Kitty}};
    std::vector<Event> events;
    contract::feed(parser, "\x1b[97", events);
    parser.configure({});
    contract::feed(parser, "uX", events);
    KeyEvent key;
    key.key = Key{U'a'};
    CHECK(events == std::vector<Event>{key, KeyEvent{U'X'}});
    contract::parses_at_every_split("x\x1b[97u", {KeyEvent{U'x'}, key},
                                    {.keyboard = KeyboardProtocol::Kitty});
}

TEST(PAR_KITTY_textless_events_and_small_replies_do_not_allocate)
{
    Parser parser{{}, {.replies = true, .keyboard = KeyboardProtocol::Kitty}};
    contract::AllocationScope allocation;
    auto key = parser.next(std::span<char const>{"\x1b[97;5:2u", 9});
    CHECK(key.event.has_value());
    auto reply = parser.next(std::span<char const>{"\x1b[?3u", 5});
    CHECK(reply.event.has_value());
    CHECK(allocation.count() == 0);
}
