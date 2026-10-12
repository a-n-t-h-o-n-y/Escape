#include "../../examples/lumen_canvas.hpp"
#include "../../examples/lumen_zoom.hpp"
#include "support/test.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <limits>
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

TEST(LUMEN_scroll_up_zooms_in_and_down_reverses_it)
{
    lumen::Zoom zoom;
    auto initial = zoom.pixel_scale(100);
    zoom.scroll(-1);
    zoom.step(1);
    CHECK(std::abs(zoom.pixel_scale(100) * 1.2L - initial) < 1e-15L);
    zoom.scroll(1);
    zoom.step(1);
    CHECK(std::abs(zoom.pixel_scale(100) - initial) < 1e-15L);
}

TEST(LUMEN_wheel_zoom_continues_beyond_thirty_times)
{
    lumen::Zoom zoom;
    zoom.scroll(-100);
    zoom.step(1);
    CHECK(std::exp(zoom.logarithm()) > 1e7L);
    auto before = zoom.pixel_scale(100);
    zoom.scroll(-1);
    zoom.step(1);
    CHECK(std::abs(zoom.pixel_scale(100) * 1.2L / before - 1) < 1e-12L);
}

TEST(LUMEN_extreme_zoom_stays_finite_and_can_zoom_back_out)
{
    lumen::Zoom zoom;
    for (unsigned i = 0; i < 10; ++i) {
        zoom.scroll(std::numeric_limits<int>::min());
        zoom.scroll(-1);
        zoom.step(0.5);
        CHECK(std::isfinite(zoom.logarithm()));
        CHECK(std::isfinite(zoom.pixel_scale(0)));
        CHECK(zoom.pixel_scale(0) >= 0);
    }
    zoom.scroll(std::numeric_limits<int>::max());
    zoom.step(1);
    CHECK(std::abs(zoom.pixel_scale(100) - 3.4L / 100 / 0.3L) < 1e-15L);
}

TEST(LUMEN_zoom_keeps_mouse_point_fixed_during_every_animation_step)
{
    lumen::JuliaView view;
    view.resize(200, 100);
    auto expected = view.point(150.5L, 69.0L);
    view.scroll(Point{150, 35}, -10);
    for (unsigned frame = 0; frame < 30; ++frame) {
        view.step(0.2);
        auto actual = view.point(150.5L, 69.0L);
        CHECK(std::abs(actual.x - expected.x) < 1e-14L);
        CHECK(std::abs(actual.y - expected.y) < 1e-14L);
    }
    // Moving the pointer while zoom is still easing replaces the anchor.
    expected = view.point(10.5L, 29.0L);
    view.scroll(Point{10, 15}, 4);
    for (unsigned frame = 0; frame < 30; ++frame) {
        view.step(0.3);
        auto actual = view.point(10.5L, 29.0L);
        CHECK(std::abs(actual.x - expected.x) < 1e-14L);
        CHECK(std::abs(actual.y - expected.y) < 1e-14L);
    }
}

TEST(LUMEN_resize_clears_old_zoom_anchor_and_preserves_view_center)
{
    lumen::JuliaView view;
    view.resize(200, 100);
    view.scroll(Point{199, 49}, -20);
    view.step(0.2);
    auto center = view.position();
    view.resize(800, 400);
    view.step(0.3);
    CHECK(view.position().x == center.x);
    CHECK(view.position().y == center.y);
    auto mapped = view.point(400, 200);
    CHECK(mapped.x == center.x && mapped.y == center.y);
}
