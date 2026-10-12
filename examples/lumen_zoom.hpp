#ifndef ESCAPE_EXAMPLES_LUMEN_ZOOM_HPP
#define ESCAPE_EXAMPLES_LUMEN_ZOOM_HPP

#include <algorithm>
#include <cmath>
#include <esc/area.hpp>
#include <limits>

namespace lumen {
class Zoom {
    long double current = 0, target = 0;

    void adjust(long double delta)
    {
        // Logarithmic zoom avoids overflowing repeated wheel/click multiplications.
        static const long double minimum = std::log(0.3L);
        static const long double maximum =
            std::log(std::numeric_limits<long double>::max()) - 1;
        target = std::clamp(target + delta, minimum, maximum);
    }

   public:
    void scroll(int steps)
    { adjust(-static_cast<long double>(steps) * std::log(1.2L)); }

    void step(double follow) { current += (target - current) * follow; }

    auto logarithm() const -> long double { return current; }

    auto pixel_scale(int height) const -> long double
    { return (3.4L / std::max(1, height)) * std::exp(-current); }
};

struct FractalPoint {
    long double x = 0, y = 0;
};

class JuliaView {
    Zoom zoom;
    FractalPoint center, anchor;
    int width = 0, height = 0;

   public:
    void resize(int columns, int pixel_rows)
    {
        width = columns;
        height = pixel_rows;
        anchor = {};
    }

    void scroll(esc::Point at, int steps)
    {
        long double x = std::clamp(at.x, 0, std::max(0, width - 1)) + 0.5L;
        long double y = std::clamp(at.y - 1, 0, std::max(0, height / 2 - 1)) * 2 + 1.0L;
        anchor = {x - width * 0.5L, y - height * 0.5L};
        zoom.scroll(steps);
    }

    void step(double follow)
    {
        auto before = pixel_scale();
        zoom.step(follow);
        auto change = before - pixel_scale();
        center.x += anchor.x * change;
        center.y += anchor.y * change;
    }

    auto pixel_scale() const -> long double { return zoom.pixel_scale(height); }

    auto logarithm() const -> long double { return zoom.logarithm(); }

    auto position() const -> FractalPoint { return center; }

    auto point(long double x, long double y) const -> FractalPoint
    {
        auto scale = pixel_scale();
        return {center.x + (x - width * 0.5L) * scale,
                center.y + (y - height * 0.5L) * scale};
    }
};
}  // namespace lumen

#endif
