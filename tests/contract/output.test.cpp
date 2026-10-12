#include "support/fixtures.hpp"
#include "support/posix.hpp"

#include <type_traits>

using namespace esc;
using namespace std::literals;

static_assert(Writable<char> && Writable<char32_t> && Writable<std::string>);
static_assert(Writable<std::u32string_view> && !Writable<int> && !Writable<bool>);
static_assert(std::is_copy_constructible_v<Output>);

TEST(OUT_01_strings_and_views_write_their_full_length)
{
    contract::File strings;
    contract::File views;
    std::string bytes{"a\0b\x1b[31m",8};
    Output{strings.get()}.write(bytes);
    Output{views.get()}.write(std::string_view{bytes});
    CHECK(strings.contents()==bytes);
    CHECK(views.contents()==bytes);
}
TEST(OUT_01_raw_bytes_do_not_validate_utf8)
{
    contract::File file;
    Output output{file.get()};
    output.write("\xff\x80\0"sv);
    CHECK(file.contents()=="\xff\x80\0"sv);
}
TEST(OUT_01_byte_and_scalar_are_distinct)
{
    contract::File file;
    Output output{file.get()};
    output.write(static_cast<char>(0xE9));
    output.write(U'é');
    CHECK(file.contents()=="\xe9\xc3\xa9"sv);
}
TEST(OUT_02_variadic_writes_keep_order_and_add_nothing)
{
    contract::File file;
    Output output{file.get()};
    output.write("a",'b',U'界',"\0c"sv,U"👽");
    output.write();
    output.flush();
    CHECK(file.contents()=="ab界\0c👽"sv);
}
TEST(OUT_02_utf32_is_validated_before_any_variadic_output)
{
    contract::File file;
    Output output{file.get()};
    CHECK_THROWS(UnicodeError,output.write("prefix",std::u32string{U'a',0xD800}));
    CHECK(file.contents().empty());
}
TEST(OUT_01_pointer_semantics_are_explicit_and_null_is_invalid)
{
    contract::File file;
    Output output{file.get()};
    char const bytes[]={'a','\0','b','\0'};
    output.write(bytes);
    output.write(std::string_view{bytes,3});
    CHECK_THROWS(std::invalid_argument,output.write(static_cast<char const*>(nullptr)));
    CHECK_THROWS(std::invalid_argument,Output{nullptr});
    CHECK(file.contents()=="aa\0b"sv);
}
TEST(OUT_02_writer_borrows_the_stream_and_copies_refer_to_it)
{
    contract::File file;
    {
        Output first{file.get()};
        auto second=first;
        first.write("a");
        second.write("b");
    }
    CHECK(::fcntl(::fileno(file.get()),F_GETFD)!=-1);
    CHECK(file.contents()=="ab");
}
TEST(OUT_02_flush_reports_delayed_stream_failure)
{
    std::array<char,4096> buffer{}; // Must outlive the borrowed FILE*'s buffering.
    contract::Pipe pipe;
    contract::Fd writer_copy{::dup(pipe.writer.get())};
    auto* raw=::fdopen(writer_copy.get(),"w");
    contract::checked(raw!=nullptr);
    writer_copy.release();
    contract::File stream{raw};
    CHECK(std::setvbuf(raw,buffer.data(),_IOFBF,buffer.size())==0);
    Output output{raw};
    output.write("buffered");
    // Keep FILE* alive but invalidate its descriptor; no signal or real terminal involved.
    CHECK(::close(::fileno(raw))==0);
    CHECK_THROWS(std::system_error,output.flush());
}
TEST(OUT_02_unbuffered_write_reports_descriptor_failure)
{
    contract::File file;
    CHECK(std::setvbuf(file.get(),nullptr,_IONBF,0)==0);
    CHECK(::close(::fileno(file.get()))==0);
    CHECK_THROWS(std::system_error,Output{file.get()}.write("bytes"));
}

TEST(OUT_01_free_stdout_functions_need_no_terminal_initialization)
{
    contract::StdoutCapture capture;
    esc::write("a",'b',U'界',"\0c"sv);
    esc::write();
    esc::flush();
    CHECK(capture.file.contents()=="ab界\0c"sv);
}
