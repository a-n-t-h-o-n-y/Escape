#pragma once

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>

namespace esc {

/**
 * The Key enum is a superset of all keys on a keyboard, including control codes.
 * @details It is designed to be used with the Mod enum to represent key combinations.
 */
enum class Key : char32_t {
    // C0 Control Codes
    Null = 0,                // Ctrl + Space, or Ctrl + 2, OR Ctrl + @
    StartOfHeading,          // Ctrl + a
    StartOfText,             // Ctrl + b
    EndOfText,               // Ctrl + c
    EndOfTransmission,       // Ctrl + d
    Enquiry,                 // Ctrl + e
    Acknowledge,             // Ctrl + f
    Bell,                    // Ctrl + g
    Backspace1,              // Ctrl + h - Not necessarily the Backspace Key.
    Tab,                     // Ctrl + i OR Tab Key
    NewLine,                 // Ctrl + j AKA Line Feed
    VerticalTab,             // Ctrl + k
    FormFeed,                // Ctrl + l
    Enter,                   // Ctrl + m - Mapped to Enter key
    ShiftOut,                // Ctrl + n
    ShiftIn,                 // Ctrl + o
    DataLinkEscape,          // Ctrl + p
    DeviceControlOne,        // Ctrl + q
    DeviceControlTwo,        // Ctrl + r
    DeviceControlThree,      // Ctrl + s
    DeviceControlFour,       // Ctrl + t
    NegativeAcknowledge,     // Ctrl + u
    SynchronousIdle,         // Ctrl + v
    EndOfTransmissionBlock,  // Ctrl + w
    Cancel,                  // Ctrl + x
    EndOfMedium,             // Ctrl + y
    Substitute,              // Ctrl + z
    Escape,                  // Ctrl + 3 OR Ctrl + [
    FileSeparator,           // Ctrl + 4 OR Ctrl + Backslash
    GroupSeparator,          // Ctrl + 5 OR Ctrl + ]
    RecordSeparator,         // Ctrl + 6 OR Ctrl + ^
    UnitSeparator,           // Ctrl + 7 OR Ctrl + _
    Backspace = 127,         // Ctrl + 8

    // Graphic Characters
    Space = 32,
    ExclamationMark,
    DoubleQuotation,
    Hash,
    Dollar,
    Percent,
    Ampersand,
    Apostrophe,
    LeftParenthesis,
    RightParenthesis,
    Asterisk,
    Plus,
    Comma,
    Minus,
    Period,
    ForwardSlash,
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
    LessThan,
    Equals,
    GreaterThan,
    QuestionMark,
    AtSign,
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
    LeftBracket,
    Backslash,
    RightBracket,
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
    LeftCurlyBracket,
    Pipe,
    RightCurlyBracket,
    Tilde = 126,

    // Special Keys - These overlap with C1 (128 - 159)
    Home = 128,
    Insert,
    Delete,
    End,
    PageUp,
    PageDown = 133,
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
    ArrowDown,  // Cursor Keys
    ArrowUp,
    ArrowLeft,
    ArrowRight,
    BackTab,     // Shift + Tab
    Backspace2,  // Numpad backspace and backspace on some laptops
    // empty
    Begin = 159,  // Middle Numpad Key

    // 2+ Byte UTF8 Starts Here
    utf8 = 160,

    // Unicode planes 4-13 are unassigned
    // Below are only used in KeyMode::Raw
    LCtrl = 0x40000,
    RCtrl,
    LShift,
    LShiftFake,
    RShift,
    RShiftFake,
    LAlt,
    RAlt,
    CapsLock,
    NumLock,
    ScrollLock,

    PrintScreen,
    PrintScreenCtrl,
    PrintScreenAlt,
    PrintScreenShift,
    Pause,
    PauseCtrl,
    CtrlBreak,
    HomeGray,
    UpGray,
    PageUpGray,
    LeftGray,
    RightGray,
    EndGray,
    DownGray,
    PageDownGray,
    InsertGray,
    DeleteGray,

    KeypadEnter,
    KeypadAsterisk,
    KeypadMinus,
    KeypadPlus,
    KeypadPeriod,
    KeypadForwardSlash,
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
    AltSystemRequest,
};

/**
 * The Mod enum is used to represent modifier keys on a keyboard.
 * @details It is designed to be used with the Key enum to represent key combinations.
 * These are only generated when KeyMode::Raw is set. In KeyMode::Normal these not seen
 * as separate keys, for instance ctrl + a is seen as Key::StartOfHeading and shift + a
 * is seen as Key::A.
 */
enum class Mod : std::underlying_type_t<Key> {
    // Last bit of unicode currently used is (1 << 20).
    Shift = (std::underlying_type_t<Key>{1} << 30),
    Ctrl = (std::underlying_type_t<Key>{1} << 29),
    Alt = (std::underlying_type_t<Key>{1} << 28),
    Meta = (std::underlying_type_t<Key>{1} << 27)
};

/**
 * Combine a Key and a Mod to create a new Key with the modifier added.
 * @param mod The modifier to add to the key.
 * @param key The key to add the modifier to.
 * @return A new Key with the modifier added.
 */
constexpr auto operator|(Mod mod, Key key) -> Key
{
    return static_cast<Key>(static_cast<std::underlying_type_t<Mod>>(mod) |
                            static_cast<std::underlying_type_t<Key>>(key));
}

/**
 * Combine a Key and a Mod to create a new Key with the modifier added.
 * @param key The key to add the modifier to.
 * @param mod The modifier to add to the key.
 * @return A new Key with the modifier added.
 */
constexpr auto operator|(Key key, Mod mod) -> Key { return ::esc::operator|(mod, key); }

/**
 * Combine two Mod enums into a single Mod enum.
 * @param a The first Mod to combine.
 * @param b The second Mod to combine.
 * @return A new Mod with both modifiers combined.
 */
constexpr auto operator|(Mod a, Mod b) -> Mod
{
    return static_cast<Mod>(static_cast<std::underlying_type_t<Mod>>(a) |
                            static_cast<std::underlying_type_t<Mod>>(b));
}

/**
 * Return true if \p mod is set on the given \p key.
 * @param key The key to check for the modifier.
 * @param mod The modifier to check for on the key.
 * @return True if the modifier is set on the key, false otherwise.
 */
constexpr auto is_set(Key key, Mod mod) -> bool
{
    return (static_cast<std::underlying_type_t<Key>>(key) &
            static_cast<std::underlying_type_t<Mod>>(mod)) > 0;
}

/**
 * Translate a char to one of the first 256 Key enum values.
 * @param c The char to translate to a Key.
 * @return The Key value that corresponds to the given char.
 */
constexpr auto char_to_key(char c) -> Key { return static_cast<Key>(c); }

/**
 * Translate a char32_t to a Key value.
 * @param c The char32_t to translate to a Key.
 * @return The Key value that corresponds to the given char32_t.
 */
constexpr auto char32_to_key(char32_t c) -> Key { return static_cast<Key>(c); }

/**
 * Translate a keycode \p k into its char representation.
 * @details Return '\0' if \p k does not have a printable representation, or it's too
 * large to fit in a single char. See key_to_char32(Key).
 * @param k The keycode to translate to a char.
 * @return The char representation of the keycode.
 */
constexpr auto key_to_char(Key k) -> char
{
    constexpr auto low = 32;
    constexpr auto high = 126;
    auto const value = static_cast<char32_t>(k);
    return (value < low || value > high) ? '\0' : static_cast<char>(value);
}

/**
 * Translate a keycode \p k into its char32_t representation.
 * @details Return U'\0' if \p k is a ctrl char or special key.
 * @param k The keycode to translate to a char32_t.
 * @return The char32_t representation of the keycode.
 */
constexpr auto key_to_char32(Key k) -> char32_t
{
    constexpr auto low = short{32};
    constexpr auto mid = short{126};
    constexpr auto high = short{160};
    auto const value = static_cast<char32_t>(k);
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
// For use in Hash-Maps.
template <>
struct hash<esc::Key> {
   public:
    using argument_type = esc::Key;
    using result_type = std::size_t;
    using underlying_t = std::underlying_type_t<argument_type>;

   public:
    auto operator()(argument_type const& key) const noexcept -> result_type
    {
        return std::hash<underlying_t>{}(static_cast<underlying_t>(key));
    }
};

}  // namespace std