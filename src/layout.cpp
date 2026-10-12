#include <algorithm>
#include <esc/detail/unicode.hpp>
#include <esc/glyph.hpp>
#include <esc/layout.hpp>
#include <limits>
#include <stdexcept>
#include "detail/layout_data.hpp"

namespace esc {
namespace {
enum class Break : unsigned {
    Other,
    CR,
    LF,
    Control,
    Extend,
    ZWJ,
    RI,
    Prepend,
    SpacingMark,
    L,
    V,
    T,
    LV,
    LVT
};
struct Properties {
    unsigned bits;
    auto boundary() const noexcept -> Break { return static_cast<Break>(bits & 15); }
    auto indic() const noexcept -> unsigned { return (bits >> 4) & 3; }
    auto pictographic() const noexcept -> bool { return bits & (1u << 6); }
    auto emoji() const noexcept -> bool { return bits & (1u << 7); }
    auto presentation() const noexcept -> bool { return bits & (1u << 8); }
    auto east_asian() const noexcept -> unsigned { return (bits >> 9) & 3; }
    auto zero() const noexcept -> bool { return bits & ((1u << 11) | (1u << 14)); }
    auto assigned() const noexcept -> bool { return bits & (1u << 12); }
    auto control() const noexcept -> bool { return bits & (1u << 13); }
};
auto properties(char32_t cp) noexcept -> Properties
{
    // ASCII bypasses all table searches.
    if (cp < 0x7f) {
        auto boundary = cp == 13   ? Break::CR
                        : cp == 10 ? Break::LF
                        : cp < 32  ? Break::Control
                                   : Break::Other;
        return {static_cast<unsigned>(boundary) | (1u << 12) |
                (cp < 32 ? 1u << 13 : 0u) |
                ((cp == '#' || cp == '*' || (cp >= '0' && cp <= '9')) ? 1u << 7 : 0u)};
    }
    auto const& ranges = detail::layout_data::ranges;
    auto it = std::lower_bound(
        std::begin(ranges), std::end(ranges), cp,
        [](auto const& range, char32_t value) { return range.last < value; });
    unsigned bits = it != std::end(ranges) && cp >= it->first ? it->properties : 0;
    if (cp >= 0xac00 && cp <= 0xd7a3)
        bits = (bits & ~15u) |
               static_cast<unsigned>((cp - 0xac00) % 28 ? Break::LVT : Break::LV);
    return {bits};
}
auto decode(std::string_view input, std::size_t& offset) -> char32_t
{ return detail::decode_utf8(input, offset); }
auto decode(std::u32string_view input, std::size_t& offset) -> char32_t
{
    auto cp = input[offset];
    detail::validate_scalar(cp, offset);
    ++offset;
    return cp;
}
auto decode(std::span<Glyph const> input, std::size_t& offset) -> char32_t
{
    auto cp = input[offset].symbol;
    detail::validate_scalar(cp, offset);
    ++offset;
    return cp;
}
auto control(Break value) noexcept -> bool
{ return value == Break::CR || value == Break::LF || value == Break::Control; }
// Constant-space left context for GB9c, GB11 and GB12/13. Never scan backwards.
struct BoundaryState {
    Break previous = Break::Other;
    bool odd_ri = false, pictographic_extend = false, pictographic_zwj = false;
    unsigned indic_chain = 0;  // 0 absent, 1 consonant, 2 consonant + linker.
    void accept(Properties p) noexcept
    {
        auto b = p.boundary();
        odd_ri = b == Break::RI && (previous != Break::RI || !odd_ri);
        pictographic_zwj = b == Break::ZWJ && pictographic_extend;
        pictographic_extend =
            p.pictographic() || (b == Break::Extend && pictographic_extend);
        if (p.indic() == 1)
            indic_chain = 1;
        else if (p.indic() == 3 && indic_chain)
            indic_chain = 2;
        else if (p.indic() != 2 && p.indic() != 3)
            indic_chain = 0;
        previous = b;
    }
    auto joins(Properties p) const noexcept -> bool
    {
        auto a = previous, b = p.boundary();
        if (a == Break::CR && b == Break::LF) return true;  // GB3
        if (control(a) || control(b)) return false;         // GB4/5
        if (a == Break::L &&
            (b == Break::L || b == Break::V || b == Break::LV || b == Break::LVT))
            return true;
        if ((a == Break::LV || a == Break::V) && (b == Break::V || b == Break::T))
            return true;
        if ((a == Break::LVT || a == Break::T) && b == Break::T) return true;
        if (b == Break::Extend || b == Break::ZWJ || b == Break::SpacingMark ||
            a == Break::Prepend)
            return true;
        if (indic_chain == 2 && p.indic() == 1) return true;
        if (pictographic_zwj && p.pictographic()) return true;
        return a == Break::RI && b == Break::RI && odd_ri;
    }
};
template <class Char>
auto scalar_unit(Char value) noexcept -> Char
{ return value; }
auto scalar_unit(Glyph const& glyph) noexcept -> char32_t { return glyph.symbol; }
template <class Char>
auto printable_ascii(Char const& value) noexcept -> bool
{ return scalar_unit(value) >= 0x20 && scalar_unit(value) <= 0x7e; }

template <class View>
auto first_size(View input) -> std::size_t
{
    if (input.empty()) return 0;
    if (input.size() > 1 && printable_ascii(input[0]) && printable_ascii(input[1]))
        return 1;
    std::size_t offset = 0;
    BoundaryState state;
    state.accept(properties(decode(input, offset)));
    while (offset < input.size()) {
        auto end = offset;
        auto p = properties(decode(input, offset));
        if (!state.joins(p)) return end;
        state.accept(p);
    }
    return offset;
}
void validate(WidthPolicy const& policy)
{
    if ((policy.ambiguous != AmbiguousWidth::One &&
         policy.ambiguous != AmbiguousWidth::Two) ||
        (policy.emoji != EmojiWidth::One && policy.emoji != EmojiWidth::Two) ||
        (policy.clusters != ClusterWidth::Maximum &&
         policy.clusters != ClusterWidth::Additive) ||
        (policy.orphan_marks != OrphanMarkWidth::Zero &&
         policy.orphan_marks != OrphanMarkWidth::One) ||
        (policy.unassigned != UnassignedWidth::One &&
         policy.unassigned != UnassignedWidth::Unknown))
        throw std::invalid_argument{"WidthPolicy: invalid enum value"};
    char32_t previous = 0;
    bool first = true;
    for (auto const& item : policy.overrides) {
        detail::validate_scalar(item.scalar, 0);
        if (item.cells > 2 || (!first && item.scalar <= previous))
            throw std::invalid_argument{
                "WidthPolicy: overrides must be sorted, unique and 0..2 cells"};
        first = false;
        previous = item.scalar;
    }
}
auto override_width(char32_t cp, WidthPolicy const& policy) noexcept
    -> std::optional<unsigned>
{
    if (policy.overrides.empty()) return {};
    auto it = std::lower_bound(
        policy.overrides.begin(), policy.overrides.end(), cp,
        [](auto const& item, char32_t value) { return item.scalar < value; });
    if (it != policy.overrides.end() && it->scalar == cp) return it->cells;
    return {};
}
void add(std::size_t& total, std::size_t value)
{
    if (value > (std::numeric_limits<std::size_t>::max)() - total)
        throw std::length_error{"Cell width overflow"};
    total += value;
}
struct ClusterMeasure {
    std::size_t maximum = 0, sum = 0;
    bool known = true, mark = false;
    unsigned keycap_state = 0;  // 1 keycap base, 2 optional VS16.
    void accept(char32_t cp, Properties p, char32_t next, WidthPolicy const& policy)
    {
        if (p.control()) {
            known = false;
            return;
        }
        auto custom = override_width(cp, policy);
        unsigned width = 0;
        if (custom)
            width = *custom;
        else if (!p.zero() && !p.assigned() &&
                 policy.unassigned == UnassignedWidth::Unknown)
            known = false;
        else if ((p.presentation() && next != 0xfe0e) || (p.emoji() && next == 0xfe0f))
            width = policy.emoji == EmojiWidth::Two ? 2 : 1;
        else if (!p.zero()) {
            width = p.east_asian() == 1 ? 2
                    : p.east_asian() == 2 && policy.ambiguous == AmbiguousWidth::Two
                        ? 2
                        : 1;
        }
        // Only the literal keycap base [#*0-9] + optional VS16 + U+20E3 is emoji.
        bool keycap = cp == 0x20e3 && keycap_state != 0;
        if (cp == '#' || cp == '*' || (cp >= '0' && cp <= '9'))
            keycap_state = 1;
        else if (cp == 0xfe0f && keycap_state == 1)
            keycap_state = 2;
        else
            keycap_state = 0;
        if (keycap && policy.clusters == ClusterWidth::Maximum && !custom) {
            width = policy.emoji == EmojiWidth::Two ? 2 : 1;
        }
        if (p.boundary() == Break::Extend || p.boundary() == Break::SpacingMark)
            mark = true;
        maximum = (std::max)(maximum, static_cast<std::size_t>(width));
        add(sum, width);
    }
    auto finish(WidthPolicy const& policy) const -> std::size_t
    {
        auto result = policy.clusters == ClusterWidth::Maximum ? maximum : sum;
        return result == 0 && mark && policy.orphan_marks == OrphanMarkWidth::One
                   ? 1
                   : result;
    }
};
template <class View>
auto measure(View input, WidthPolicy const& policy, bool single)
    -> std::optional<std::size_t>
{
    validate(policy);
    if (input.empty()) return 0;
    std::size_t offset = 0, total = 0, count = 1;
    bool known = true;
    auto cp = decode(input, offset);
    auto p = properties(cp);
    BoundaryState state;
    state.accept(p);
    ClusterMeasure cluster;
    while (true) {
        bool has_next = offset < input.size();
        auto next = has_next ? decode(input, offset) : char32_t{};
        cluster.accept(cp, p, next, policy);
        auto next_properties = has_next ? properties(next) : Properties{};
        if (!has_next || !state.joins(next_properties)) {
            known = known && cluster.known;
            add(total, cluster.finish(policy));
            if (!has_next) break;
            ++count;
            cluster = {};
            state = {};
            // Consume proven single-cell ASCII clusters. Keep the last scalar
            // pending: a following mark or VS16 can still join and change it.
            if (policy.overrides.empty() && printable_ascii(next)) {
                while (offset < input.size() && printable_ascii(input[offset])) {
                    add(total, 1);
                    ++count;
                    next = static_cast<char32_t>(scalar_unit(input[offset++]));
                }
                next_properties = properties(next);
            }
        }
        state.accept(next_properties);
        cp = next;
        p = next_properties;
    }
    if (single && count != 1)
        throw std::invalid_argument{"grapheme_width: input contains multiple clusters"};
    return known ? std::optional<std::size_t>{total} : std::nullopt;
}
}  // namespace
auto next_grapheme(std::string_view input) -> std::string_view
{ return input.substr(0, first_size(input)); }
auto next_grapheme(std::u32string_view input) -> std::u32string_view
{ return input.substr(0, first_size(input)); }
auto cell_width(std::string_view input, WidthPolicy policy)
    -> std::optional<std::size_t>
{ return measure(input, policy, false); }
auto cell_width(std::u32string_view input, WidthPolicy policy)
    -> std::optional<std::size_t>
{ return measure(input, policy, false); }
auto grapheme_width(std::string_view input, WidthPolicy policy)
    -> std::optional<std::size_t>
{ return measure(input, policy, true); }
auto grapheme_width(std::u32string_view input, WidthPolicy policy)
    -> std::optional<std::size_t>
{ return measure(input, policy, true); }
auto next_grapheme(std::span<Glyph const> input) -> std::span<Glyph const>
{ return input.first(first_size(input)); }
auto cell_width(std::span<Glyph const> input, WidthPolicy policy)
    -> std::optional<std::size_t>
{ return measure(input, policy, false); }
auto grapheme_width(std::span<Glyph const> input, WidthPolicy policy)
    -> std::optional<std::size_t>
{ return measure(input, policy, true); }
}  // namespace esc
