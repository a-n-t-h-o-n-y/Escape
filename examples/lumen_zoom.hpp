#ifndef ESCAPE_EXAMPLES_LUMEN_ZOOM_HPP
#define ESCAPE_EXAMPLES_LUMEN_ZOOM_HPP

#include <algorithm>
#include <cmath>
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

    void magnify() { adjust(std::log(1.4L)); }

    void step(double follow) { current += (target - current) * follow; }

    auto logarithm() const -> long double { return current; }

    auto pixel_scale(int height) const -> long double
    { return (3.4L / std::max(1, height)) * std::exp(-current); }
};
}  // namespace lumen

#endif
