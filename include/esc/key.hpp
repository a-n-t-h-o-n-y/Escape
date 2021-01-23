#ifndef ESC_KEY_HPP
#define ESC_KEY_HPP
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>

namespace esc {

enum class Key : short {
    // Control Characters
    Null = 0,    // Ctrl + Space, or Ctrl + 2, OR Ctrl + @
    Ctrl_a,      // Start of heading
    Ctrl_b,      // Start of text
    Ctrl_c,      // End of text
    Ctrl_d,      // End of transmission
    Ctrl_e,      // Enquiry
    Ctrl_f,      // Acknowledge
    Ctrl_g,      // Bell
    Backspace_,  // Not used? ascii value 8 check with keypad
    Tab,         // Ctrl + I
    Enter,       // Ctrl + J, or Ctrl + M, AKA Line Feed, AKA Carriage Return
    Ctrl_k,      // Vertical Tab
    Ctrl_l,      // Form Feed
    Carriage_return_,     // old, not on modern keyboards
    Ctrl_n,               // Shift Out
    Ctrl_o,               // Shift In
    Ctrl_p,               // Data Link Escape
    Ctrl_q,               // Device Control One
    Ctrl_r,               // Device Control Two
    Ctrl_s,               // Device Control Three
    Ctrl_t,               // Device Control Four
    Ctrl_u,               // Negative Acknowledge
    Ctrl_v,               // Synchronous Idle
    Ctrl_w,               // End of Transmission Block
    Ctrl_x,               // Cancel
    Ctrl_y,               // End of Medium
    Ctrl_z,               // Substitute
    Escape,               // Ctrl + [, or Ctrl + 3
    Ctrl_backslash,       // Ctrl + 4, File Separator
    Ctrl_closed_bracket,  // Ctrl + 5, Group Separator
    Ctrl_caret,           // Ctrl + 6, Record Separator
    Ctrl_underscore,      // Ctrl + 7, Unit Separator
    Backspace = 127,      // Ctrl + 8

    // Printable Characters
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
    Vertical_bar,  // pipe?
    Right_curly_bracket,
    Tilde,

    // Curses Specialty Characters
    Arrow_down = 258,
    Arrow_up,
    Arrow_left,
    Arrow_right,
    Home,
    Backspace_2,  // numpad backspace and backspace on some laptops

    Function = 264,
    Function1,
    Function2,
    Function3,
    Function4,
    Function5,
    Function6,
    Function7,
    Function8,
    Function9,
    Function10,
    Function11,
    Function12,

    Delete_line = 328,
    Insert_line,
    Delete,
    Insert,
    EIC,  // sent by rmir or smir in insert mode
    Clear_screen,
    Clear_to_end_of_screen,
    Clear_to_end_of_line,
    Scroll_forward,
    Scroll_backward,
    Next_page,
    Previous_page,
    Set_tab,
    Clear_tab,
    Clear_all_tabs,
    Possibly_keypad_enter_,  // int 343

    Page_up,
    Page_down,

    Print = 346,
    Home_down,
    Keypad_7,
    Keypad_9,
    Keypad_5,
    Keypad_1,
    Keypad_3,
    Back_tab,
    Begin,
    Cancel_dup_,
    Close,
    Command,
    Copy,
    Create,
    End,
    Exit,
    Find,
    Help,
    Mark,
    Message,
    Move,
    Next,
    Open,
    Options,
    Previous,
    Redo,
    Reference,
    Refresh,
    Replace,
    Restart,
    Resume,
    Save,
    Shift_begin,
    Shift_cancel,
    Shift_command,
    Shift_copy,
    Shift_create,
    Shift_delete_character,
    Shift_delete_line,
    Select,
    Shift_end,
    Shift_clear_to_end_of_line,
    Shift_exit,
    Shift_find,
    Shift_help,
    Shift_home,
    Shift_insert_character,
    Shift_left_arrow,
    Shift_message,
    Shift_move,
    Shift_next,
    Shift_options,
    Shift_previous,
    Shift_print,
    Shift_redo,
    Shift_replace,
    Shift_right_arrow,
    Shift_resume,
    Shift_save,
    Shift_suspend,
    Shift_undo,
    Suspend,
    Undo,
    Invalid,  // 409
};

/// Translate a char(below value 127) to a Key value; Key::Invalid otherwise.
inline auto char_to_key(char c) -> Key
{
    return c > 127 ? Key::Invalid : static_cast<Key>(c);
}

/// Translate a keycode \p k into its char representation.
/** Return '\0' if \p k does not have a printable representation. */
inline auto key_to_char(Key k) -> char
{
    auto constexpr low  = short{32};
    auto constexpr high = short{126};
    auto const value    = static_cast<short>(k);
    return (value < low || value > high) ? '\0' : static_cast<char>(value);
}

/// Translate a keycode \p k into its char32_t representation.
/** Return U'\0' if \p k does not have a printable representation. */
inline auto key_to_char32_t(Key k) -> char32_t
{
    return static_cast<char32_t>(key_to_char(k));
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
