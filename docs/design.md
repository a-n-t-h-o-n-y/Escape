# Escape Library

Provides escape sequence strings for ANSI and XTERM compliant terminal
emulators. Input works for UNIX like systems only.

API:

## `esc/device.hpp`

```cpp
namespace esc {

struct Mouse{};

struct Key{};

struct Window{};

using No_device = std::monostate;
type alias for std::monostate?

using Device = std::variant<Mouse, Key, Window, No_device>;

} // namespace esc
```

## `esc/io.hpp`

```cpp
namespace esc::io {

/// Alt screen and locale to utf8
void initialize_stdout();

/// Normal screen
void uninitialize_stdout();

/// Writes a single byte to the console via stdout.
inline void write(char c);

/// Writes a 4 byte char to the console via stdout.
/** First converts the character to a multi-byte array via std::c32rtomb() and
  * sends each resulting char to write(). The conversion uses the current C
  * locale. */
inline void write(std::char32_t c);

/// Writes each char of \p sv to the console via stdout.
inline void write(std::string_view sv);

/// Writes each std::char32_t of \p sv to the console via stdout.
/** Calls write(std::char32_t) for each element of \p sv. */
inline void write(std::u32string_view sv);

/// Send all buffered bytes from calls to write(...) to the console device.
inline void flush();


/// set up timeout for reads? setup mouse mode, sigwinch handler, read one char
/// at a time from stdin.
void initialize_stdin();
// 1002 to DECSET enables mouse input and mouse movement tracking.
// 1005 is utf8 mode which means you can get coords past 223, you have to do
// this, but you also have to make sure to read as utf8 mode then.

/// Blocking read of a single Event from stdin.
auto read() -> input_device_variant;

/// Reads a single Event from stdin, or returns std::monostate if timeout passes
/// without any chars written to stdin.
template <typename Rep, typename Period>
auto read(std::chrono::Duration<Rep, Period> timeout) -> input_device_variant;

/// Initializes both stdout and stdin.
inline void intitialize();

/// Uninitializes stdout.
inline void unintitialize();

} // namespace esc::io
```

## `esc/esc.hpp`

```cpp
namespace esc {

struct Position{};

enum Trait;

struct Color{};

inline void move_cursor(Position p);

inline void add_trait(Trait t);

inline void remove_trait(Trait t);

inline void add_background(Color c);

inline void add_foreground(Color c);

inline void remove_background();

inline void remove_foreground();

inline void alternate_screen_buffer();

inline void normal_screen_buffer();

} // namespace esc
```

## `esc/debug.hpp`

```cpp

namespace esc::debug {

// functions to print out struct representation of devices.

// function to print out control sequences in readable format, translates the
// esc char to a string of chars \033 or something.

} // namespace esc::debug
```

# Testing

```cpp
#include <esc/esc.hpp>

int main() {
    esc::initialize();
    auto count = 0;
    while (count < 4) {
        auto const e = esc::read();
        std::visit([](auto const& x){ esc::debug::print(x, std::cerr); }, e);
        ++count;
    }
    esc::uninitialize();
}
```
