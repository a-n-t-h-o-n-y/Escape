#pragma once
#include <chrono>
#include <concepts>
#include <cstdio>
#include <esc/detail/unicode.hpp>
#include <esc/parser.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace esc {
template <class T>
concept Writable =
    std::same_as<std::remove_cvref_t<T>, char> ||
    std::same_as<std::remove_cvref_t<T>, char32_t> ||
    std::same_as<std::remove_cvref_t<T>, std::string> ||
    std::same_as<std::remove_cvref_t<T>, std::string_view> ||
    std::same_as<std::remove_cvref_t<T>, std::u32string> ||
    std::same_as<std::remove_cvref_t<T>, std::u32string_view> ||
    std::same_as<std::remove_cvref_t<T>, char*> ||
    std::same_as<std::remove_cvref_t<T>, char const*> ||
    (std::is_array_v<std::remove_reference_t<T>> &&
     (std::same_as<std::remove_cv_t<std::remove_extent_t<std::remove_reference_t<T>>>,
                   char> ||
      std::same_as<std::remove_cv_t<std::remove_extent_t<std::remove_reference_t<T>>>,
                   char32_t>));

class Output {
   public:
    explicit Output(std::FILE* stream = stdout);
    template <Writable... Values>
    void write(Values const&... values)
    {
        (validate(values), ...);
        (write_one(values), ...);
    }
    void flush();

   private:
    std::FILE* stream_;
    static void validate(char) noexcept {}
    static void validate(char32_t value) { detail::validate_scalar(value, 0); }
    static void validate(std::string_view) noexcept {}
    static void validate(std::u32string_view value) { detail::validate_utf32(value); }
    static void validate(char const* value)
    {
        if (!value) throw std::invalid_argument{"Output: null string"};
    }
    void write_one(char byte);
    void write_one(char32_t scalar);
    void write_one(std::string_view bytes);
    void write_one(std::u32string_view text);
    void write_one(char const* text) { write_one(std::string_view{text}); }
};
template <Writable... Values>
void write(Values const&... values)
{ Output{}.write(values...); }
void flush();

enum class ReadStatus { Event, Timeout, EndOfInput };
struct ReadResult {
    ReadStatus status;
    std::optional<Event> event;
};
class Reader {
   public:
    explicit Reader(std::FILE* input = stdin,
                    ParserOptions options = {},
                    InputProtocols protocols = {});
    ~Reader();
    Reader(Reader&&) noexcept;
    auto operator=(Reader&&) noexcept -> Reader&;
    Reader(Reader const&) = delete;
    auto operator=(Reader const&) -> Reader& = delete;
    auto read() -> ReadResult;
    auto read(std::chrono::milliseconds timeout) -> ReadResult;
    void configure(InputProtocols protocols);

   private:
    friend class Terminal;
    void prepare_queries();
    void stop_queries() noexcept;
    void configure_session(InputProtocols protocols,
                           MouseMode mode,
                           int output_descriptor);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace esc
