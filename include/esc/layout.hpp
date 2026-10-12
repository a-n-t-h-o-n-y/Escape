#pragma once

#include <cstddef>
#include <cstdint>
#include <esc/text.hpp>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>

namespace esc {
struct Glyph;
inline constexpr std::string_view layout_unicode_version = "17.0.0";

// Return the first extended grapheme cluster, borrowing input. Empty input gives
// an empty view. Strict decoding validates the cluster and its boundary lookahead.
[[nodiscard]] auto next_grapheme(std::string_view input) -> std::string_view;

[[nodiscard]] auto next_grapheme(std::u32string_view input) -> std::u32string_view;

// Styled scalar spans borrow their original glyphs; brushes do not change boundaries.
[[nodiscard]] auto next_grapheme(std::span<Glyph const> input)
    -> std::span<Glyph const>;

template <class Char>
class GraphemeView : public std::ranges::view_interface<GraphemeView<Char>> {
    static_assert(std::same_as<Char, char> || std::same_as<Char, char32_t>);

   public:
    using View = std::basic_string_view<Char>;

    class Iterator {
       public:
        using value_type = View;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;

        Iterator() = default;

        [[nodiscard]] auto operator*() const noexcept -> View { return current_; }

        auto operator++() -> Iterator&
        {
            offset_ += current_.size();
            select();
            return *this;
        }

        auto operator++(int) -> Iterator
        {
            auto old = *this;
            ++*this;
            return old;
        }

        friend auto operator==(Iterator const& a, Iterator const& b) noexcept -> bool
        {
            return a.input_.data() == b.input_.data() &&
                   a.input_.size() == b.input_.size() && a.offset_ == b.offset_;
        }

        friend auto operator==(Iterator const& a, std::default_sentinel_t) noexcept
            -> bool
        { return a.offset_ == a.input_.size(); }

       private:
        friend class GraphemeView;

        explicit Iterator(View input) : input_{input} { select(); }

        void select()
        {
            // Two printable ASCII scalars always have a boundary between them.
            // Keep non-ASCII lookahead on the full path: it may join the last
            // ASCII scalar (combining mark, keycap, variation selector, ...).
            if (input_.size() - offset_ > 1 && input_[offset_] >= 0x20 &&
                input_[offset_] <= 0x7e && input_[offset_ + 1] >= 0x20 &&
                input_[offset_ + 1] <= 0x7e) {
                current_ = input_.substr(offset_, 1);
                return;
            }
            try {
                current_ = next_grapheme(input_.substr(offset_));
            }
            catch (UnicodeError const& error) {
                throw UnicodeError{offset_ + error.offset()};
            }
        }

        View input_{}, current_{};
        std::size_t offset_ = 0;
    };

    GraphemeView() = default;

    explicit GraphemeView(View input) noexcept : input_{input} {}

    [[nodiscard]] auto begin() const -> Iterator { return Iterator{input_}; }

    [[nodiscard]] auto end() const noexcept -> std::default_sentinel_t { return {}; }

   private:
    View input_{};
};

[[nodiscard]] inline auto graphemes(std::string_view input) noexcept
    -> GraphemeView<char>
{ return GraphemeView<char>{input}; }

[[nodiscard]] inline auto graphemes(std::u32string_view input) noexcept
    -> GraphemeView<char32_t>
{ return GraphemeView<char32_t>{input}; }

enum class AmbiguousWidth { One, Two };

enum class EmojiWidth { One, Two };

enum class ClusterWidth { Maximum, Additive };

enum class OrphanMarkWidth { Zero, One };

enum class UnassignedWidth { One, Unknown };

struct ScalarWidthOverride {
    char32_t scalar;
    std::uint8_t cells;  // 0..2; unique scalars in ascending order.
};

struct WidthPolicy {
    AmbiguousWidth ambiguous = AmbiguousWidth::One;
    EmojiWidth emoji = EmojiWidth::Two;
    ClusterWidth clusters = ClusterWidth::Maximum;
    OrphanMarkWidth orphan_marks = OrphanMarkWidth::Zero;
    UnassignedWidth unassigned = UnassignedWidth::One;
    std::span<ScalarWidthOverride const> overrides{};
};

// A deterministic estimate under the supplied policy, without I/O or locale.
// nullopt means cursor-dependent controls or an unknown unassigned scalar.
// Invalid encoding/policy throws. The complete input is always validated.
[[nodiscard]] auto cell_width(std::string_view input, WidthPolicy policy = {})
    -> std::optional<std::size_t>;

[[nodiscard]] auto cell_width(std::u32string_view input, WidthPolicy policy = {})
    -> std::optional<std::size_t>;

// Empty => 0; more than one cluster => invalid_argument.
[[nodiscard]] auto grapheme_width(std::string_view cluster, WidthPolicy policy = {})
    -> std::optional<std::size_t>;

[[nodiscard]] auto grapheme_width(std::u32string_view cluster, WidthPolicy policy = {})
    -> std::optional<std::size_t>;

[[nodiscard]] auto cell_width(std::span<Glyph const> input, WidthPolicy policy = {})
    -> std::optional<std::size_t>;

[[nodiscard]] auto grapheme_width(std::span<Glyph const> cluster,
                                  WidthPolicy policy = {})
    -> std::optional<std::size_t>;
}  // namespace esc

namespace std::ranges {
template <class Char>
inline constexpr bool enable_borrowed_range<esc::GraphemeView<Char>> = true;
}
