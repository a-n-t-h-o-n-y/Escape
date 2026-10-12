#ifndef ESCAPE_EXAMPLES_LUMEN_CANVAS_HPP
#define ESCAPE_EXAMPLES_LUMEN_CANVAS_HPP

#include "lumen_workers.hpp"

#include <algorithm>
#include <esc/esc.hpp>
#include <memory>
#include <string>
#include <vector>

namespace lumen {
// Keep the last displayed pixels so sparse scenes only repaint changed cells.
class Canvas {
    struct Cell {
        esc::RGB top{0, 0, 0}, bottom{0, 0, 0};
        auto operator==(Cell const&) const -> bool = default;
    };

    std::vector<Cell> cells;
    esc::Area size{};
    bool parallel;
    std::unique_ptr<RowWorkers> workers;
    std::vector<std::string> row_bytes;

   public:
    explicit Canvas(bool parallel = true) : parallel{parallel} {}

    template <class Scene>
    void draw(Scene const& scene, esc::Area area, std::string& bytes, bool full)
    {
        int columns = std::max(0, area.width - 1);
        int rows = std::max(0, area.height - 2);
        if (area != size) {
            size = area;
            cells.resize(std::size_t(columns) * rows);
            full = true;
        }
        auto draw_row = [&](int y, std::string& output) {
            bool adjacent = false, styled = false;
            Cell style;
            for (int x = 0; x < columns; ++x) {
                Cell pixel{scene.sample(x + 0.5, y * 2 + 0.5),
                           scene.sample(x + 0.5, y * 2 + 1.5)};
                auto& previous = cells[std::size_t(y) * columns + x];
                if (!full && pixel == previous) {
                    adjacent = false;
                    continue;
                }
                if (!adjacent) esc::append_encoded(output, esc::Cursor{x, y + 1});
                bool foreground = !styled || pixel.top != style.top;
                bool background = !styled || pixel.bottom != style.bottom;
                if (foreground && background)
                    esc::append_encoded(output, esc::fg(pixel.top),
                                        esc::bg(pixel.bottom));
                else if (foreground)
                    esc::append_encoded(output, esc::fg(pixel.top));
                else if (background)
                    esc::append_encoded(output, esc::bg(pixel.bottom));
                output += "▀";
                previous = style = pixel;
                adjacent = styled = true;
            }
        };
        // Small frames avoid thread dispatch and extra row-buffer copies.
        if (!parallel || cells.size() < 32'000) {
            for (int y = 0; y < rows; ++y)
                draw_row(y, bytes);
        }
        else {
            if (!workers) workers = std::make_unique<RowWorkers>();
            row_bytes.resize(rows);
            workers->run(rows, [&](int y) {
                auto& output = row_bytes[y];
                output.clear();
                draw_row(y, output);
            });
            for (auto const& output : row_bytes)
                bytes += output;
        }
    }
};
}  // namespace lumen

#endif
