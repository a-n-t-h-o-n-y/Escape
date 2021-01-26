#ifndef ESC_KEY_HPP
#define ESC_KEY_HPP
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>

namespace esc {

/// Represents Keys on a keyboard, along with control codes they generate.
enum class Key : char32_t {
    // C0 Control Codes
    Null = 0,                   // Ctrl + Space, or Ctrl + 2, OR Ctrl + @
    Start_of_heading,           // Ctrl + a
    Start_of_text,              // Ctrl + b
    End_of_text,                // Ctrl + c
    End_of_transmission,        // Ctrl + d
    Enquiry,                    // Ctrl + e
    Acknowledge,                // Ctrl + f
    Bell,                       // Ctrl + g
    Backspace_1,                // Ctrl + h - Not necessarily the Backspace Key.
    Tab,                        // Ctrl + i OR Tab Key
    Enter,                      // Ctrl + j OR Ctrl + m OR Enter - AKA Line Feed
    Vertical_tab,               // Ctrl + k
    Form_feed,                  // Ctrl + l
    Carriage_return_,           // Ctrl + m - Usually not mapped to Enter key
    Shift_out,                  // Ctrl + n
    Shift_in,                   // Ctrl + o
    Data_link_escape,           // Ctrl + p
    Device_control_one,         // Ctrl + q
    Device_control_two,         // Ctrl + r
    Device_control_three,       // Ctrl + s
    Device_control_four,        // Ctrl + t
    Negative_acknowledge,       // Ctrl + u
    Synchronous_idle,           // Ctrl + v
    End_of_transmission_block,  // Ctrl + w
    Cancel,                     // Ctrl + x
    End_of_medium,              // Ctrl + y
    Substitute,                 // Ctrl + z
    Escape,                     // Ctrl + 3 OR Ctrl + [
    File_separator,             // Ctrl + 4 OR Ctrl + Backslash
    Group_separator,            // Ctrl + 5 OR Ctrl + ]
    Record_separator,           // Ctrl + 6 OR Ctrl + ^
    Unit_separator,             // Ctrl + 7 OR Ctrl + _
    Backspace = 127,            // Ctrl + 8

    // Graphic Characters
    Space = 32,
    Exclamation_mark,
    Double_quotation,
    Hash,
    Dollar,
    Percent,
    Ampersand,
    Apostrophe,
    Left_parenthesis,
    Right_parenthesis,
    Asterisk,
    Plus,
    Comma,
    Minus,
    Period,
    Forward_slash,
    Zero,
    One,
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven,
    Eight,
    Nine,
    Colon,
    Semicolon,
    Less_than,
    Equals,
    Greater_than,
    Question_mark,
    At_sign,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Left_bracket,
    Backslash,
    Right_bracket,
    Caret,
    Underscore,
    Accent,
    a,
    b,
    c,
    d,
    e,
    f,
    g,
    h,
    i,
    j,
    k,
    l,
    m,
    n,
    o,
    p,
    q,
    r,
    s,
    t,
    u,
    v,
    w,
    x,
    y,
    z,
    Left_curly_bracket,
    Pipe,
    Right_curly_bracket,
    Tilde = 126,

    // Special Keys - These overlap with C1 (128 - 159)
    Home = 128,
    Insert,
    Delete,
    End,
    Page_up,
    Page_down = 133,
    // empty
    // empty
    // empty
    // empty
    Function1 = 138,
    Function2,
    Function3,
    Function4,
    Function5 = 142,
    // empty
    Function6 = 144,
    Function7,
    Function8,
    Function9,
    Function10 = 148,
    // empty
    Function11 = 150,
    Function12,
    Arrow_down,  // Cursor Keys
    Arrow_up,
    Arrow_left,
    Arrow_right,
    Back_tab,     // Shift + Tab
    Backspace_2,  // Numpad backspace and backspace on some laptops
    // empty
    Begin = 159,  // Middle Numpad Key

    // 2+ Byte UTF8 Starts Here
    utf8 = 160
};

/// Translate a char to one of the first 256 Key enum values.
inline auto char_to_key(char c) -> Key { return static_cast<Key>(c); }

/// Translate a char32_t to a Key value.
inline auto char32_to_key(char32_t c) -> Key { return static_cast<Key>(c); }

/// Translate a keycode \p k into its char representation.
/** Return '\0' if \p k does not have a printable representation, or it's too
 *  large to fit in a single char. See key_to_char32(Key). */
inline auto key_to_char(Key k) -> char
{
    auto constexpr low  = 32;
    auto constexpr high = 126;
    auto const value    = static_cast<char32_t>(k);
    return (value < low || value > high) ? '\0' : static_cast<char>(value);
}

/// Translate a keycode \p k into its char32_t representation.
/** Return '\0' if \p k is a ctrl char or special key. */
inline auto key_to_char32(Key k) -> char32_t
{
    auto constexpr low  = short{32};
    auto constexpr mid  = short{126};
    auto constexpr high = short{256};
    auto const value    = static_cast<char32_t>(k);
    return (value < low || (value > mid && value < high))
               ? U'\0'
               : static_cast<char32_t>(value);
}

/// Key code enum with modifier keys that were held down at time of keypress.
struct Key_press {
    /// Key Pressed
    Key key;

    /// Keyboard Modifiers
    struct Modifiers {
        bool shift = false;
        bool ctrl  = false;
        bool alt   = false;
        bool meta  = false;
    } modifiers = {};
};

constexpr auto operator==(Key_press const& a, Key_press const& b) -> bool
{
    return std::tie(a.key, a.modifiers.alt, a.modifiers.ctrl, a.modifiers.meta,
                    a.modifiers.shift) ==
           std::tie(b.key, b.modifiers.alt, b.modifiers.ctrl, b.modifiers.meta,
                    b.modifiers.shift);
}

constexpr auto operator!=(Key_press const& a, Key_press const& b) -> bool
{
    return !(a == b);
}

}  // namespace esc

namespace std {

/// For use in Hash-Maps.
template <>
struct hash<esc::Key> {
   public:
    using argument_type = esc::Key;
    using result_type   = std::size_t;
    using underlying_t  = std::underlying_type_t<argument_type>;

   public:
    auto operator()(argument_type const& key) const noexcept -> result_type
    {
        return std::hash<underlying_t>{}(static_cast<underlying_t>(key));
    }
};

}  // namespace std
#endif  // ESC_KEY_HPP
