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
    New_line,                   // Ctrl + j AKA Line Feed
    Vertical_tab,               // Ctrl + k
    Form_feed,                  // Ctrl + l
    Enter,                      // Ctrl + m - Mapped to Enter key
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
    utf8 = 160,

    // Unicode planes 4-13 are unassigned
    // Below are only used in Key_mode::Raw
    LCtrl = 0x40000,
    RCtrl,
    LShift,
    LShift_fake,
    RShift,
    RShift_fake,
    LAlt,
    RAlt,
    CapsLock,
    NumLock,
    ScrollLock,

    Print_screen,
    Print_screen_ctrl,
    Print_screen_alt,
    Print_screen_shift,
    Pause,
    Pause_ctrl,
    Ctrl_break,
    Home_gray,
    Up_gray,
    Page_up_gray,
    Left_gray,
    Right_gray,
    End_gray,
    Down_gray,
    Page_down_gray,
    Insert_gray,
    Delete_gray,

    Keypad_enter,
    Keypad_asterisk,
    Keypad_minus,
    Keypad_plus,
    Keypad_period,
    Keypad_forward_slash,
    Keypad0,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,
    Alt_system_request,
};

/// Modifier Keys
enum class Mod : std::underlying_type_t<Key> {
    // Last bit of unicode currently used is (1 << 20).
    Shift = (char32_t{1} << 30),
    Ctrl  = (char32_t{1} << 29),
    Alt   = (char32_t{1} << 28),
    Meta  = (char32_t{1} << 27)
};

/// Insert \p mod into \p key and return a new Key with the modifier added.
auto constexpr operator|(Mod mod, Key key) -> Key
{
    return static_cast<Key>(static_cast<std::underlying_type_t<Mod>>(mod) |
                            static_cast<std::underlying_type_t<Key>>(key));
}

/// Insert \p mod into \p key and return a new Key with the modifier added.
auto constexpr operator|(Key key, Mod mod) -> Key
{
    return esc::operator|(mod, key);
}

/// Insert \p mod into \p key and return a new Key with the modifier added.
auto constexpr operator|(Mod a, Mod b) -> Mod
{
    return static_cast<Mod>(static_cast<std::underlying_type_t<Mod>>(a) |
                            static_cast<std::underlying_type_t<Mod>>(b));
}

/// Return true if \p mod is set on the given \p key.
auto constexpr is_set(Key key, Mod mod) -> bool
{
    return (static_cast<std::underlying_type_t<Key>>(key) &
            static_cast<std::underlying_type_t<Mod>>(mod)) > 0;
}

/// Translate a char to one of the first 256 Key enum values.
auto constexpr char_to_key(char c) -> Key { return static_cast<Key>(c); }

/// Translate a char32_t to a Key value.
auto constexpr char32_to_key(char32_t c) -> Key { return static_cast<Key>(c); }

/// Translate a keycode \p k into its char representation.
/** Return '\0' if \p k does not have a printable representation, or it's too
 *  large to fit in a single char. See key_to_char32(Key). */
auto constexpr key_to_char(Key k) -> char
{
    auto constexpr low  = 32;
    auto constexpr high = 126;
    auto const value    = static_cast<char32_t>(k);
    return (value < low || value > high) ? '\0' : static_cast<char>(value);
}

/// Translate a keycode \p k into its char32_t representation.
/** Return '\0' if \p k is a ctrl char or special key. */
auto constexpr key_to_char32(Key k) -> char32_t
{
    auto constexpr low  = short{32};
    auto constexpr mid  = short{126};
    auto constexpr high = short{160};
    auto const value    = static_cast<char32_t>(k);
    if (is_set(k, Mod::Shift) || is_set(k, Mod::Ctrl) || is_set(k, Mod::Alt) ||
        is_set(k, Mod::Meta)) {
        return U'\0';
    }
    return (value < low || (value > mid && value < high))
               ? U'\0'
               : static_cast<char32_t>(value);
}

}  // namespace esc

namespace std {

// Required for gcc < 6.1 and sometimes clang 7?

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
