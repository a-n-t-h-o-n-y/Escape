#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <esc/esc.hpp>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#endif
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#ifdef _MSC_VER
#    include <intrin.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
volatile std::uint64_t checksum = 0;

void observe(char const* data, std::size_t size)
{
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "r"(data), "r"(size) : "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
    if (size)
        checksum = checksum ^
                   static_cast<unsigned char>(*static_cast<char const volatile*>(data));
    _ReadWriteBarrier();
#else
    if (size)
        checksum = checksum ^
                   static_cast<unsigned char>(*static_cast<char const volatile*>(data));
#endif
}

struct Case {
    std::string name;
    std::size_t bytes, units;
    std::function<std::uint64_t(std::size_t)> run;
};

struct Sink {
    std::FILE* stream;
    std::array<char, 8192> buffer{};

    explicit Sink(bool buffered)
    {
#ifdef _WIN32
        stream = std::fopen("NUL", "wb");
#else
        stream = std::fopen("/dev/null", "wb");
#endif
        if (!stream) throw std::runtime_error{"Cannot open null output sink"};
        if (std::setvbuf(stream, buffered ? buffer.data() : nullptr,
                         buffered ? _IOFBF : _IONBF, buffered ? buffer.size() : 0)) {
            std::fclose(stream);
            throw std::runtime_error{"Cannot configure sink buffering"};
        }
    }

    ~Sink() { std::fclose(stream); }
};

auto frame() -> std::string
{
    return esc::encode(esc::Cursor{123, 45}, esc::fg(esc::RGB{0x38a9df}),
                       esc::Trait::Bold, esc::BlankRow{});
}

auto parse(esc::Parser& parser,
           std::string_view bytes,
           std::size_t fragment,
           std::vector<esc::Event>* captured = nullptr) -> std::size_t
{
    std::size_t events = 0;
    while (!bytes.empty()) {
        auto chunk = bytes.substr(0, std::min(fragment, bytes.size()));
        std::size_t offset = 0;
        while (offset < chunk.size()) {
            auto result = parser.next(
                std::span<char const>{chunk.data() + offset, chunk.size() - offset});
            offset += result.consumed;
            if (result.event) {
                ++events;
                if (captured) captured->push_back(std::move(*result.event));
            }
            if (!result.consumed && !result.event)
                throw std::runtime_error{"Parser made no progress"};
        }
        bytes.remove_prefix(chunk.size());
    }
    // A final paste chunk may be pending without additional input.
    while (auto event = parser.next({}).event) {
        ++events;
        if (captured) captured->push_back(std::move(*event));
    }
    return events;
}

void require(bool condition)
{
    if (!condition)
        throw std::runtime_error{"Benchmark fixture failed its correctness preflight"};
}
}  // namespace

int main(int argc, char** argv)
{
    try {
        double milliseconds = 100;
        std::size_t samples = 7;
        std::string filter;
        for (int i = 1; i < argc; ++i) {
            auto arg = std::string_view{argv[i]};
            if (arg == "--help") {
                std::cout << "Usage: escape.benchmarks [--milliseconds N] [--samples "
                             "N] [--filter substring]\n";
                return 0;
            }
            if (++i == argc) throw std::invalid_argument{"Missing option value"};
            if (arg == "--milliseconds") {
                std::size_t used;
                milliseconds = std::stod(argv[i], &used);
                require(used == std::string_view{argv[i]}.size());
            }
            else if (arg == "--samples") {
                std::size_t used;
                auto value = std::stod(argv[i], &used);
                require(used == std::string_view{argv[i]}.size() &&
                        std::isfinite(value) && value >= 1 && value <= 101 &&
                        std::floor(value) == value);
                samples = static_cast<std::size_t>(value);
            }
            else if (arg == "--filter")
                filter = argv[i];
            else
                throw std::invalid_argument{"Unknown option"};
        }
        if (!std::isfinite(milliseconds) || milliseconds < 1 || milliseconds > 10000)
            throw std::invalid_argument{"Milliseconds must be between 1 and 10000"};
        auto expected = frame();
        require(expected == "\x1b[46;124H\x1b[38;2;56;169;223m\x1b[1m\x1b[2K");
        std::string reused;
        reused.reserve(128);
        std::array<char, 128> storage{};
        auto encoded =
            esc::encode_to(storage, esc::Cursor{123, 45}, esc::fg(esc::RGB{0x38a9df}),
                           esc::Trait::Bold, esc::BlankRow{});
        require(encoded.written == expected.size() &&
                std::string_view{storage.data(), encoded.written} == expected);
        esc::append_encoded(reused, esc::Cursor{123, 45}, esc::fg(esc::RGB{0x38a9df}),
                            esc::Trait::Bold, esc::BlankRow{});
        require(reused == expected);
        std::array<std::string, 2> layout_inputs = {
            std::string(256, 'a'),
            esc::utf32_to_utf8(U"e\u0301中\U0001f469\u200d\U0001f4bb\U0001f1fa"
                               U"\U0001f1f8\u0915\u094d\u0937")};
        require(esc::cell_width(layout_inputs[0]) == 256);
        require(esc::cell_width(layout_inputs[1]) == 8);
        std::vector<Case> cases;
        std::u32string text_input = U"ASCII e\u0301中\U0001f469\u200d\U0001f4bb ";
        while (text_input.size() < 1024)
            text_input += text_input;
        auto text_bytes = esc::utf32_to_utf8(text_input);
        std::string text_output;
        text_output.reserve(text_bytes.size());
        cases.push_back(
            {"text/reserved-utf32-to-utf8", text_bytes.size(), 1, [&](std::size_t n) {
                 std::uint64_t total = 0;
                 while (n--) {
                     text_output.clear();
                     esc::append_utf8(text_output, text_input);
                     observe(text_output.data(), text_output.size());
                     total += text_output.size();
                 }
                 return total;
             }});
        std::string title(4096, 't');
        std::string title_output;
        title_output.reserve(title.size() + 6);
        require(esc::encode(esc::WindowTitle{title}) == "\x1b]2;" + title + "\x1b\\");
        cases.push_back({"encode/title-4KiB", title.size() + 6, 1, [&](std::size_t n) {
                             std::uint64_t total = 0;
                             while (n--) {
                                 title_output.clear();
                                 esc::append_encoded(title_output,
                                                     esc::WindowTitle{title});
                                 observe(title_output.data(), title_output.size());
                                 total += title_output.size();
                             }
                             return total;
                         }});
        for (std::size_t index = 0; index < layout_inputs.size(); ++index) {
            auto const name = index == 0 ? "ascii" : "mixed";
            auto const expected_clusters = index == 0 ? 256u : 5u;
            std::size_t preflight_clusters = 0;
            for (auto cluster :
                 esc::graphemes(std::string_view{layout_inputs[index]})) {
                require(!cluster.empty());
                ++preflight_clusters;
            }
            require(preflight_clusters == expected_clusters);
            cases.push_back({std::string{"layout/width/"} + name,
                             layout_inputs[index].size(), 1, [&, index](std::size_t n) {
                                 std::uint64_t total = 0;
                                 for (std::size_t i = 0; i < n; ++i) {
                                     auto const& input = layout_inputs[index];
                                     observe(input.data(), input.size());
                                     auto cells = esc::cell_width(input);
                                     total += *cells;
                                 }
                                 return total;
                             }});
            cases.push_back({std::string{"layout/graphemes/"} + name,
                             layout_inputs[index].size(), 1, [&, index](std::size_t n) {
                                 std::uint64_t total = 0;
                                 for (std::size_t i = 0; i < n; ++i) {
                                     auto const& input = layout_inputs[index];
                                     observe(input.data(), input.size());
                                     for (auto cluster :
                                          esc::graphemes(std::string_view{input}))
                                         total += cluster.size();
                                 }
                                 return total;
                             }});
        }
        cases.push_back({"encode/string", expected.size(), 1, [&](std::size_t n) {
                             std::uint64_t total = 0;
                             while (n--) {
                                 auto bytes = frame();
                                 observe(bytes.data(), bytes.size());
                                 total += bytes.size();
                             }
                             return total;
                         }});
        cases.push_back(
            {"encode/reserved-append", expected.size(), 1, [&](std::size_t n) {
                 std::uint64_t total = 0;
                 while (n--) {
                     reused.clear();
                     esc::append_encoded(reused, esc::Cursor{123, 45},
                                         esc::fg(esc::RGB{0x38a9df}), esc::Trait::Bold,
                                         esc::BlankRow{});
                     observe(reused.data(), reused.size());
                     total += reused.size();
                 }
                 return total;
             }});
        cases.push_back({"encode/span", expected.size(), 1, [&](std::size_t n) {
                             std::uint64_t total = 0;
                             while (n--) {
                                 auto r =
                                     esc::encode_to(storage, esc::Cursor{123, 45},
                                                    esc::fg(esc::RGB{0x38a9df}),
                                                    esc::Trait::Bold, esc::BlankRow{});
                                 observe(storage.data(), r.written);
                                 total += r.written;
                             }
                             return total;
                         }});
        auto profile =
            esc::resolve_profile({"xterm-256color", "truecolor", false}, true).profile;
        require(esc::encode(profile, esc::Cursor{123, 45}, esc::fg(esc::RGB{0x38a9df}),
                            esc::Trait::Bold, esc::BlankRow{}) == expected);
        cases.push_back(
            {"encode/profile-truecolor", expected.size(), 1, [&](std::size_t n) {
                 std::uint64_t total = 0;
                 while (n--) {
                     auto r = esc::encode_to(storage, profile, esc::Cursor{123, 45},
                                             esc::fg(esc::RGB{0x38a9df}),
                                             esc::Trait::Bold, esc::BlankRow{});
                     observe(storage.data(), r.written);
                     total += r.written;
                 }
                 return total;
             }});
        auto basic = profile;
        auto indexed = profile;
        indexed.colors = esc::ColorSupport::Indexed256;
        require(esc::encode(indexed, esc::fg(esc::RGB{0x38a9df})) == "\x1b[38;5;74m");
        cases.push_back({"encode/profile-256-color", 10, 1, [&](std::size_t n) {
                             std::uint64_t total = 0;
                             while (n--) {
                                 auto r = esc::encode_to(storage, indexed,
                                                         esc::fg(esc::RGB{0x38a9df}));
                                 observe(storage.data(), r.written);
                                 total += r.written;
                             }
                             return total;
                         }});
        std::array<esc::RGB, 8> varied_colors = {
            esc::RGB{0x38a9df}, esc::RGB{0x5f87af}, esc::RGB{0x080808},
            esc::RGB{0x8a8a8a}, esc::RGB{0xff0000}, esc::RGB{0x00ffff},
            esc::RGB{0xeeddcc}, esc::RGB{0x005500}};
        std::array<std::string_view, 8> varied_expected = {
            "\x1b[38;5;74m", "\x1b[38;5;67m", "\x1b[38;5;232m", "\x1b[38;5;245m",
            "\x1b[91m",      "\x1b[96m",      "\x1b[38;5;224m", "\x1b[38;5;22m"};
        std::size_t varied_bytes = 0;
        for (std::size_t i = 0; i < varied_colors.size(); ++i) {
            require(esc::encode(indexed, esc::fg(varied_colors[i])) ==
                    varied_expected[i]);
            varied_bytes += varied_expected[i].size();
        }
        cases.push_back({"encode/profile-256-varied", varied_bytes,
                         varied_colors.size(), [&](std::size_t n) {
                             std::uint64_t total = 0;
                             while (n--)
                                 for (auto color : varied_colors) {
                                     auto r = esc::encode_to(storage, indexed,
                                                             esc::fg(color));
                                     observe(storage.data(), r.written);
                                     total += r.written;
                                 }
                             return total;
                         }});
        basic.colors = esc::ColorSupport::Basic16;
        auto basic_bytes = esc::encode(basic, esc::fg(esc::RGB{0x38a9df}));
        cases.push_back(
            {"encode/profile-16-color", basic_bytes.size(), 1, [&](std::size_t n) {
                 std::uint64_t total = 0;
                 while (n--) {
                     auto r =
                         esc::encode_to(storage, basic, esc::fg(esc::RGB{0x38a9df}));
                     observe(storage.data(), r.written);
                     total += r.written;
                 }
                 return total;
             }});
        std::string mixed =
            "a\xe7\x95\x8c\x1b[A\x1b[1;5D\x1b[<0;4;2M\x1b[<0;4;2m\x1b[I";
        esc::InputProtocols protocols{esc::MouseProtocol::Sgr, true, true};
        esc::Parser parser{{}, protocols};
        for (auto fragment : {mixed.size(), std::size_t{1}}) {
            parser.reset();
            std::vector<esc::Event> captured;
            require(parse(parser, mixed, fragment, &captured) == 7);
            std::vector<esc::Event> golden{
                esc::KeyEvent{U'a'},
                esc::KeyEvent{U'界'},
                esc::KeyEvent{esc::KeyCode::ArrowUp},
                esc::KeyEvent{esc::KeyCode::ArrowLeft, {.ctrl = true}},
                esc::MousePress{{3, 1}, esc::MouseButton::Left},
                esc::MouseRelease{{3, 1}, esc::MouseButton::Left},
                esc::Focus{true}};
            require(captured == golden);
        }
        for (auto fragment : {mixed.size(), std::size_t{1}})
            cases.push_back(
                {fragment == 1 ? "parse/mixed-byte-fragments" : "parse/mixed-whole",
                 mixed.size(), 7, [&, fragment](std::size_t n) {
                     std::uint64_t total = 0;
                     while (n--) {
                         parser.reset();
                         total += parse(parser, mixed, fragment);
                     }
                     return total;
                 }});
        std::string paste = "\x1b[200~" + std::string(4096, 'p') + "\x1b[201~";
        parser.reset();
        std::vector<esc::Event> captured_paste;
        auto chunks = parse(parser, paste, 64, &captured_paste);
        require(chunks > 0);
        std::string payload;
        for (auto const& event : captured_paste) {
            auto const& chunk = std::get<esc::PasteChunk>(event);
            require(!chunk.aborted);
            payload += chunk.bytes;
        }
        require(payload == std::string(4096, 'p') &&
                std::get<esc::PasteChunk>(captured_paste.front()).first &&
                std::get<esc::PasteChunk>(captured_paste.back()).last);
        cases.push_back(
            {"parse/paste-4KiB-fragments", paste.size(), 1, [&](std::size_t n) {
                 std::uint64_t total = 0;
                 while (n--) {
                     parser.reset();
                     total += parse(parser, paste, 64);
                 }
                 return total;
             }});
        std::unique_ptr<std::FILE, decltype(&std::fclose)> input{std::tmpfile(),
                                                                 &std::fclose};
        if (!input) throw std::runtime_error{"Cannot create reader fixture"};
#ifdef _WIN32
        if (::_setmode(::_fileno(input.get()), _O_BINARY) < 0)
            throw std::runtime_error{"Cannot set binary reader fixture"};
#endif
        std::string file_bytes(1024 * 1024, 'x');
        require(std::fwrite(file_bytes.data(), 1, file_bytes.size(), input.get()) ==
                file_bytes.size());
        require(std::fflush(input.get()) == 0);
        std::rewind(input.get());
        {
            esc::Reader reader{input.get()};
            auto result = reader.read(std::chrono::milliseconds{0});
            require(result == esc::ReadResult{esc::Event{esc::KeyEvent{U'x'}}});
        }
        cases.push_back(
            {"reader/ready-file-ASCII", 1, 1, [&](std::size_t n) {
                 std::rewind(input.get());
                 esc::Reader reader{input.get()};
                 std::uint64_t total = 0;
                 while (n) {
                     auto result = reader.read(std::chrono::milliseconds{0});
                     if (std::holds_alternative<esc::EndOfInput>(result)) {
                         std::rewind(input.get());
                         reader = esc::Reader{input.get()};
                         continue;
                     }
                     if (!std::holds_alternative<esc::Event>(result))
                         throw std::runtime_error{"Ready file did not produce input"};
                     total += static_cast<std::uint64_t>(std::get<char32_t>(
                         *std::get<esc::KeyEvent>(std::get<esc::Event>(result)).key));
                     --n;
                 }
                 return total;
             }});
        Sink buffered{true}, unbuffered{false};
        esc::Output output{buffered.stream}, direct{unbuffered.stream};
        std::string small(32, 'x'), large(4096, 'x');
        auto add_output = [&](std::string name, esc::Output& sink,
                              std::string const& bytes) {
            cases.push_back(
                {std::move(name), bytes.size(), 1, [&sink, &bytes](std::size_t n) {
                     auto count = n;
                     while (n--)
                         sink.write(std::string_view{bytes});
                     sink.flush();
                     return static_cast<std::uint64_t>(count) * bytes.size();
                 }});
        };
        add_output("output/buffered-32B", output, small);
        add_output("output/unbuffered-32B", direct, small);
        add_output("output/buffered-4KiB", output, large);
        std::cout << "Escape benchmarks: steady_clock; median and min/max; " << samples
                  << " samples; target " << milliseconds << " ms/sample\n";
#ifdef NDEBUG
        std::cout << "Build: optimized (NDEBUG)\n";
#else
        std::cout << "Build: debug; use Release for comparisons\n";
#endif
        std::cout << "case,iterations/sample,median_ns/unit,min_ns/unit,max_ns/"
                     "unit,MiB/s\n";
        std::size_t selected = 0;
        for (auto const& benchmark : cases) {
            if (benchmark.name.find(filter) == std::string::npos) continue;
            ++selected;
            checksum = benchmark.run(16);  // warm-up, outside measurements
            std::size_t iterations = 1;
            for (int attempt = 0; attempt < 12; ++attempt) {
                auto start = Clock::now();
                checksum = benchmark.run(iterations);
                auto elapsed =
                    std::chrono::duration<double, std::milli>(Clock::now() - start)
                        .count();
                if (elapsed >= milliseconds * 0.8 || iterations >= 100000000) break;
                auto factor =
                    std::clamp(milliseconds / std::max(elapsed, 0.001), 2.0, 10.0);
                iterations = std::min<std::size_t>(
                    100000000, static_cast<std::size_t>(iterations * factor));
            }
            std::vector<double> times;
            for (std::size_t sample = 0; sample < samples; ++sample) {
                auto start = Clock::now();
                auto value = benchmark.run(iterations);
                auto end = Clock::now();
                checksum = value;
                times.push_back(
                    std::chrono::duration<double, std::nano>(end - start).count() /
                    iterations / benchmark.units);
            }
            std::sort(times.begin(), times.end());
            auto median = (times[(samples - 1) / 2] + times[samples / 2]) / 2;
            auto mib = benchmark.bytes / static_cast<double>(benchmark.units) / median *
                       1e9 / (1024 * 1024);
            std::cout << benchmark.name << ',' << iterations << ',' << std::fixed
                      << std::setprecision(2) << median << ',' << times.front() << ','
                      << times.back() << ',' << mib << '\n';
        }
        if (!selected) throw std::invalid_argument{"Filter selected no benchmarks"};
        std::cout << "checksum=" << checksum << '\n';
        return 0;
    }
    catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
