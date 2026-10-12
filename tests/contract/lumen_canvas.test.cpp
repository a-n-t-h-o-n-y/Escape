#include "../../examples/lumen_canvas.hpp"
#include "support/test.hpp"

#include <array>
#include <atomic>
#include <vector>

using namespace esc;

namespace {
struct Pattern {
    std::vector<RGB> pixels = std::vector<RGB>(32, RGB{0});

    auto sample(double x, double y) const -> RGB
    { return pixels[static_cast<int>(y) * 4 + static_cast<int>(x)]; }
};

// The canvas reserves one column and the header/footer rows.
constexpr Area area{5, 4};
}  // namespace

TEST(LUMEN_first_frame_and_forced_redraw_include_black_pixels)
{
    lumen::Canvas canvas;
    Pattern scene;
    std::string bytes;
    canvas.draw(scene, area, bytes, false);
    auto expected = encode(Cursor{0, 1}, fg(RGB{0}), bg(RGB{0}), "▀▀▀▀", Cursor{0, 2},
                           fg(RGB{0}), bg(RGB{0}), "▀▀▀▀");
    CHECK(bytes == expected);
    bytes.clear();
    canvas.draw(scene, area, bytes, false);
    CHECK(bytes.empty());
    canvas.draw(scene, area, bytes, true);
    CHECK(bytes == expected);
}

TEST(LUMEN_changes_across_gaps_preserve_foreground_and_background)
{
    lumen::Canvas canvas;
    Pattern scene;
    std::string bytes;
    canvas.draw(scene, area, bytes, false);
    scene.pixels[0] = scene.pixels[3] = RGB{0x123456};
    scene.pixels[7] = RGB{0xABCDEF};
    scene.pixels[10] = RGB{0x654321};
    bytes.clear();
    canvas.draw(scene, area, bytes, false);
    CHECK(bytes == encode(Cursor{0, 1}, fg(RGB{0x123456}), bg(RGB{0}), "▀",
                          Cursor{3, 1}, bg(RGB{0xABCDEF}), "▀", Cursor{2, 2},
                          fg(RGB{0x654321}), bg(RGB{0}), "▀"));
    // Erase a star that moved away, including its lower half.
    scene.pixels[3] = scene.pixels[7] = RGB{0};
    bytes.clear();
    canvas.draw(scene, area, bytes, false);
    CHECK(bytes == encode(Cursor{3, 1}, fg(RGB{0}), bg(RGB{0}), "▀"));
}

TEST(LUMEN_adjacent_changes_share_cursor_and_style)
{
    lumen::Canvas canvas;
    Pattern scene;
    std::string bytes;
    canvas.draw(scene, area, bytes, false);
    scene.pixels[1] = scene.pixels[2] = RGB{0x112233};
    bytes.clear();
    canvas.draw(scene, area, bytes, false);
    CHECK(bytes == encode(Cursor{1, 1}, fg(RGB{0x112233}), bg(RGB{0}), "▀▀"));
}

TEST(LUMEN_resize_invalidates_cache_even_with_same_cell_count)
{
    lumen::Canvas canvas;
    Pattern scene;
    std::string bytes;
    canvas.draw(scene, area, bytes, false);
    bytes.clear();
    canvas.draw(scene, Area{3, 6}, bytes, false);
    auto expected = std::string{};
    for (int row = 1; row <= 4; ++row)
        append_encoded(expected, Cursor{0, row}, fg(RGB{0}), bg(RGB{0}), "▀▀");
    CHECK(bytes == expected);
    bytes.clear();
    canvas.draw(scene, Area{1, 1}, bytes, false);
    CHECK(bytes.empty());
    canvas.draw(scene, Area{5, 2}, bytes, false);
    CHECK(bytes.empty());
    canvas.draw(scene, area, bytes, false);
    CHECK(!bytes.empty());
}

TEST(LUMEN_large_parallel_frames_match_serial_output)
{
    struct Gradient {
        unsigned phase = 0;

        auto sample(double x, double y) const -> RGB
        {
            unsigned column = static_cast<unsigned>(x), row = static_cast<unsigned>(y);
            // Include both dense color changes and an unchanged background.
            if ((column + row) % 7 != 0) return RGB{0x030507};
            return {static_cast<std::uint8_t>(column + phase),
                    static_cast<std::uint8_t>(row + phase), 127};
        }
    } scene;

    lumen::Canvas serial{false}, parallel;
    for (auto size : {Area{401, 122}, Area{601, 82}, Area{5, 4}, Area{401, 122}}) {
        for (unsigned frame = 0; frame < 5; ++frame) {
            std::string expected, actual;
            bool full = frame == 3;
            serial.draw(scene, size, expected, full);
            parallel.draw(scene, size, actual, full);
            CHECK(actual == expected);
            if (frame == 1) CHECK(actual.empty());
            if (frame != 0) ++scene.phase;
        }
    }
}

TEST(LUMEN_worker_errors_propagate_and_pool_can_be_reused)
{
    lumen::RowWorkers workers;
    CHECK_THROWS(std::runtime_error, workers.run(100, [](int row) {
        if (row == 2) throw std::runtime_error{"render failure"};
    }));
    std::array<std::atomic<unsigned>, 100> counts{};
    for (unsigned frame = 0; frame < 20; ++frame)
        workers.run(100, [&](int row) { ++counts[row]; });
    for (auto const& count : counts)
        CHECK(count == 20);
}
