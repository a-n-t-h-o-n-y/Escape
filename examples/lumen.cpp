#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <esc/esc.hpp>
#include <iostream>
#include <numbers>
#include <random>
#include <string>
#include <variant>
#include <vector>

using namespace esc;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace {
struct Vec {
    double x = 0, y = 0;
};

struct Orb {
    Vec at, velocity;
    double radius = 1;
};

struct Ripple {
    Vec at;
    double age = 0;
};

struct Light {
    double red = 0, green = 0, blue = 0;

    auto operator+(Light b) const -> Light
    { return {red + b.red, green + b.green, blue + b.blue}; }

    auto operator*(double scale) const -> Light
    { return {red * scale, green * scale, blue * scale}; }
};

auto mix(Light a, Light b, double t) -> Light { return a * (1 - t) + b * t; }

auto palette(double t) -> Light
{
    constexpr double tau = 2 * std::numbers::pi;
    return {0.55 + 0.45 * std::cos(tau * (t + 0.00)),
            0.55 + 0.45 * std::cos(tau * (t + 0.33)),
            0.55 + 0.45 * std::cos(tau * (t + 0.67))};
}

auto rgb(Light light) -> RGB
{
    auto channel = [](double value) {
        return static_cast<std::uint8_t>(
            std::lround(std::clamp(value, 0.0, 1.0) * 255));
    };
    return {channel(light.red), channel(light.green), channel(light.blue)};
}

class Liquid {
    std::array<Orb, 7> orbs{};
    std::vector<Ripple> ripples;
    Vec target;
    int width = 0, height = 0;
    double time = 0, hue = 0.57;
    bool following = false;

   public:
    void resize(int columns, int rows)
    {
        width = columns;
        height = rows * 2;
        target = {width * 0.5, height * 0.5};
        for (unsigned i = 0; i < orbs.size(); ++i)
            orbs[i] = {target, {}, height * (0.19 - i * 0.019)};
        ripples.clear();
        following = false;
    }

    void aim(Point at)
    {
        target = {std::clamp(double(at.x), 0.0, double(width)),
                  std::clamp(double((at.y - 1) * 2), 0.0, double(height))};
        following = true;
    }

    void splash(Point at)
    {
        aim(at);
        if (ripples.size() == 12) ripples.erase(ripples.begin());
        ripples.push_back({target});
    }

    void shift(int steps) { hue += steps * 0.035; }

    void step(double elapsed)
    {
        // Small integration steps keep the spring stable after slow frames.
        elapsed = std::min(elapsed, 0.1);
        while (elapsed > 0) {
            double dt = std::min(elapsed, 1.0 / 120);
            time += dt;
            if (!following)
                target = {width * (0.5 + 0.22 * std::sin(time * 0.67)),
                          height * (0.5 + 0.19 * std::sin(time * 0.91))};
            Vec destination = target;
            for (auto& orb : orbs) {
                orb.velocity.x += (destination.x - orb.at.x) * 95 * dt;
                orb.velocity.y += (destination.y - orb.at.y) * 95 * dt;
                double drag = std::exp(-12 * dt);
                orb.velocity.x *= drag;
                orb.velocity.y *= drag;
                orb.at.x += orb.velocity.x * dt;
                orb.at.y += orb.velocity.y * dt;
                destination = orb.at;
            }
            for (auto& ripple : ripples)
                ripple.age += dt;
            elapsed -= dt;
        }
        std::erase_if(ripples, [](Ripple const& ripple) { return ripple.age > 2; });
    }

    auto sample(double x, double y) const -> RGB
    {
        double field = 0, nx = 0, ny = 0;
        for (auto const& orb : orbs) {
            double dx = x - orb.at.x, dy = y - orb.at.y;
            double radius2 = orb.radius * orb.radius;
            double denominator = dx * dx + dy * dy + radius2 * 0.35;
            field += radius2 / denominator;
            nx += radius2 * dx / (denominator * denominator);
            ny += radius2 * dy / (denominator * denominator);
        }
        double wave =
            std::sin(x * 0.055 + time * 0.32 + std::sin(y * 0.075 - time * 0.4));
        Light color = Light{0.012, 0.018, 0.045} +
                      palette(hue + wave * 0.07) * (0.022 + 0.018 * wave);
        double glow = std::min(field * field * 0.018, 0.3);
        color = color + palette(hue + y * 0.002) * glow;
        if (field > 0.9) {
            double angle = std::atan2(ny, nx);
            Light surface = palette(hue + std::sin(angle + 0.45) * 0.14 + time * 0.035 +
                                    std::sin(field * 2 - time) * 0.035);
            double edge = std::clamp((field - 0.9) * 6, 0.0, 1.0);
            double light = 0.55 + 0.35 * std::cos(angle + 2.2);
            double rim = std::exp(-std::abs(field - 1.05) * 4);
            double specular = std::pow(std::max(0.0, std::cos(angle + 2.2)), 16) *
                              std::exp(-std::abs(field - 1.7) * 1.5);
            surface = surface * light + Light{0.5, 0.8, 1} * rim * 0.4 +
                      Light{1, 1, 1} * specular * 0.65;
            color = mix(color, surface, edge);
        }
        for (auto const& ripple : ripples) {
            double distance = std::hypot(x - ripple.at.x, y - ripple.at.y);
            double ring = distance - ripple.age * height * 0.55;
            color = color + palette(hue + ripple.age * 0.2) *
                                (std::exp(-ring * ring * 0.4) *
                                 std::pow(1 - ripple.age / 2, 2) * 0.8);
        }
        return rgb(color);
    }
};

class Tide {
    int width = 0, height = 0;
    std::vector<double> water, previous, next;
    double accumulator = 0, time = 0, rain = 0, strength = 1;

    void disturb(Vec at, double amount)
    {
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx) {
                int x = static_cast<int>(at.x) + dx, y = static_cast<int>(at.y) + dy;
                if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) continue;
                auto index = std::size_t(y) * width + x;
                water[index] = std::clamp(
                    water[index] + amount * std::exp(-(dx * dx + dy * dy) * 0.3), -5.0,
                    5.0);
            }
    }

   public:
    void resize(int columns, int rows)
    {
        width = columns;
        height = rows * 2;
        water.assign(std::size_t(width) * height, 0);
        previous = next = water;
        accumulator = rain = 0;
        disturb({width * 0.5, height * 0.5}, 2);
    }

    void aim(Point at)
    { disturb({double(at.x), double((at.y - 1) * 2)}, 0.15 * strength); }

    void splash(Point at)
    { disturb({double(at.x), double((at.y - 1) * 2)}, 2 * strength); }

    void shift(int steps) { strength = std::clamp(strength + steps * 0.15, 0.2, 3.0); }

    void step(double elapsed)
    {
        accumulator += std::min(elapsed, 0.1);
        while (accumulator >= 1.0 / 90) {
            accumulator -= 1.0 / 90;
            time += 1.0 / 90;
            rain += 1.0 / 90;
            if (rain > 0.65) {
                disturb({width * (0.5 + 0.35 * std::sin(time * 2.31)),
                         height * (0.5 + 0.35 * std::cos(time * 1.73))},
                        strength);
                rain = 0;
            }
            for (int y = 1; y < height - 1; ++y)
                for (int x = 1; x < width - 1; ++x) {
                    auto i = std::size_t(y) * width + x;
                    double laplacian = water[i - 1] + water[i + 1] + water[i - width] +
                                       water[i + width] - 4 * water[i];
                    next[i] = (2 * water[i] - previous[i] + 0.23 * laplacian) * 0.993;
                }
            previous.swap(water);
            water.swap(next);
        }
    }

    auto sample(double x, double y) const -> RGB
    {
        int column = std::clamp(static_cast<int>(x), 0, width - 1);
        int row = std::clamp(static_cast<int>(y), 0, height - 1);
        auto i = std::size_t(row) * width + column;
        double slope_x =
            column > 0 && column < width - 1 ? water[i + 1] - water[i - 1] : 0;
        double slope_y =
            row > 0 && row < height - 1 ? water[i + width] - water[i - width] : 0;
        double shine =
            std::pow(std::max(0.0, 1 - std::abs(slope_x + slope_y - 0.55)), 8);
        double caustic = std::pow(0.5 + 0.5 * std::sin(x * 0.22 + slope_x * 5) *
                                            std::cos(y * 0.22 + slope_y * 5),
                                  4);
        Light color = Light{0.015, 0.12, 0.17} + Light{0.03, 0.25, 0.29} * caustic +
                      Light{0.2, 0.5, 0.55} * std::min(std::abs(water[i]) * 0.3, 1.0) +
                      Light{0.65, 0.95, 1} * shine * 0.7;
        return rgb(color);
    }
};

class Warp {
    struct Star {
        double x, y, z;
    };

    std::vector<Star> stars;
    std::vector<Light> pixels;
    std::mt19937 random{42};
    int width = 0, height = 0;
    Vec target, camera;
    double speed = 0.35, burst = 0;

    auto coordinate() -> double
    { return std::uniform_real_distribution<double>{-1, 1}(random); }

    void paint(int x, int y, Light color)
    {
        if (x >= 0 && x < width && y >= 0 && y < height)
            pixels[std::size_t(y) * width + x] =
                pixels[std::size_t(y) * width + x] + color;
    }

   public:
    void resize(int columns, int rows)
    {
        width = columns;
        height = rows * 2;
        pixels.resize(std::size_t(width) * height);
        stars.resize(std::clamp(std::size_t(width) * height / 12, std::size_t{150},
                                std::size_t{6000}));
        for (auto& star : stars)
            star = {coordinate(), coordinate(), (coordinate() + 1) * 0.45 + 0.1};
        target = camera = {};
    }

    void aim(Point at)
    {
        target = {(at.x - width * 0.5) / std::max(1, width),
                  ((at.y - 1) * 2 - height * 0.5) / std::max(1, height)};
    }

    void splash(Point at)
    {
        aim(at);
        burst = 1;
    }

    void shift(int steps) { speed = std::clamp(speed + steps * 0.07, 0.05, 1.5); }

    void step(double elapsed)
    {
        double dt = std::min(elapsed, 0.1);
        double follow = 1 - std::exp(-dt * 5);
        camera.x += (target.x - camera.x) * follow;
        camera.y += (target.y - camera.y) * follow;
        burst *= std::exp(-dt * 2);
        for (auto& star : stars) {
            star.z -= dt * (speed + burst * 2);
            if (star.z < 0.08) star = {coordinate(), coordinate(), 1};
        }
    }

    void prepare()
    {
        std::fill(pixels.begin(), pixels.end(), Light{0.005, 0.008, 0.025});
        for (auto const& star : stars) {
            double x = star.x - camera.x, y = star.y - camera.y;
            double px = width * 0.5 + x * height * 0.6 / star.z;
            double py = height * 0.5 + y * height * 0.6 / star.z;
            double old_z = star.z + 0.012 * speed + burst * 0.1;
            double tail_x = width * 0.5 + x * height * 0.6 / old_z;
            double tail_y = height * 0.5 + y * height * 0.6 / old_z;
            int length = std::clamp(
                static_cast<int>(std::hypot(px - tail_x, py - tail_y)), 1, 80);
            Light color =
                mix(Light{0.3, 0.55, 1}, Light{1, 0.8, 0.55}, (star.x + 1) * 0.5);
            for (int k = 0; k <= length; ++k) {
                double t = double(k) / length;
                paint(static_cast<int>(tail_x + (px - tail_x) * t),
                      static_cast<int>(tail_y + (py - tail_y) * t),
                      color * ((1 - star.z) * (0.2 + 0.8 * t)));
            }
        }
    }

    auto sample(double x, double y) const -> RGB
    { return rgb(pixels[std::size_t(y) * width + static_cast<int>(x)]); }
};

class Julia {
    int width = 0, height = 0;
    Vec parameter{-0.745, 0.186}, target = parameter, center;
    double time = 0, zoom = 1, target_zoom = 1;
    bool following = false;

   public:
    void resize(int columns, int rows)
    {
        width = columns;
        height = rows * 2;
    }

    void aim(Point at)
    {
        target = {-0.745 + (double(at.x) / std::max(1, width) - 0.5) * 0.35,
                  0.186 + (double((at.y - 1) * 2) / std::max(1, height) - 0.5) * 0.4};
        following = true;
    }

    void splash(Point at)
    {
        double scale = 3.4 / (std::max(1, height) * zoom);
        center.x += (at.x - width * 0.5) * scale;
        center.y += ((at.y - 1) * 2 - height * 0.5) * scale;
        target_zoom = std::min(target_zoom * 1.4, 30.0);
    }

    void shift(int steps)
    { target_zoom = std::clamp(target_zoom * std::pow(1.2, steps), 0.3, 30.0); }

    void step(double elapsed)
    {
        double dt = std::min(elapsed, 0.1);
        time += dt;
        if (!following)
            target = {-0.745 + std::sin(time * 0.3) * 0.025,
                      0.186 + std::cos(time * 0.23) * 0.03};
        double follow = 1 - std::exp(-dt * 4);
        parameter.x += (target.x - parameter.x) * follow;
        parameter.y += (target.y - parameter.y) * follow;
        zoom += (target_zoom - zoom) * follow;
    }

    auto sample(double x, double y) const -> RGB
    {
        double scale = 3.4 / (std::max(1, height) * zoom);
        double zx = center.x + (x - width * 0.5) * scale;
        double zy = center.y + (y - height * 0.5) * scale;
        unsigned iteration = 0;
        double magnitude = 0;
        while (iteration < 40 && (magnitude = zx * zx + zy * zy) < 256) {
            double next_x = zx * zx - zy * zy + parameter.x;
            zy = 2 * zx * zy + parameter.y;
            zx = next_x;
            ++iteration;
        }
        if (iteration == 40) return RGB{0x08091C};
        double smooth =
            iteration + 1 - std::log2(std::log2(std::max(magnitude, 1.001)));
        double glow = std::clamp(smooth / 12, 0.05, 1.0);
        return rgb(palette(smooth * 0.045 + time * 0.015 + 0.6) * glow);
    }
};

using Demo = std::variant<Liquid, Tide, Warp, Julia>;
constexpr std::array<std::string_view, 4> titles = {"Liquid", "Tide", "Warp", "Julia"};
constexpr std::array<std::string_view, 4> controls = {
    "mouse steer | click ripple | wheel color",
    "mouse disturb | click splash | wheel strength",
    "mouse steer | click boost | wheel speed",
    "mouse morph | click recenter | wheel zoom"};

template <class Scene>
void render(Terminal& terminal,
            Scene& scene,
            Area area,
            bool paused,
            unsigned selected,
            std::string& bytes)
{
    bytes.clear();
    if constexpr (requires { scene.prepare(); }) scene.prepare();
    int available = std::max(0, area.width - 1);
    auto title = std::string{" LUMEN "};
    title.resize(std::min(title.size(), std::size_t(available)));
    append_encoded(bytes, ResetStyle{}, Cursor{}, bg(RGB{0x03050B}), BlankRow{},
                   fg(RGB{0x9EDBFF}), title);
    int column = 7;
    for (unsigned i = 0; i < titles.size() && column < available; ++i) {
        std::string label = " " + std::to_string(i + 1) + " " + std::string{titles[i]};
        label.resize(12, ' ');
        label.resize(std::min(label.size(), std::size_t(available - column)));
        append_encoded(bytes, bg(RGB{i == selected ? 0x204E69u : 0x03050Bu}),
                       fg(RGB{i == selected ? 0xE0F7FFu : 0x7893AAu}), label);
        column += 12;
    }
    int columns = std::max(0, area.width - 1);
    int rows = std::max(0, area.height - 2);
    for (int y = 0; y < rows; ++y) {
        append_encoded(bytes, Cursor{0, y + 1});
        RGB previous_top{0, 0, 0}, previous_bottom{0, 0, 0};
        for (int x = 0; x < columns; ++x) {
            auto top = scene.sample(x + 0.5, y * 2 + 0.5);
            auto bottom = scene.sample(x + 0.5, y * 2 + 1.5);
            if (x == 0 || top != previous_top) append_encoded(bytes, fg(top));
            if (x == 0 || bottom != previous_bottom) append_encoded(bytes, bg(bottom));
            append_encoded(bytes, "▀");
            previous_top = top;
            previous_bottom = bottom;
        }
    }
    if (area.height > 1) {
        std::string hint = paused ? " PAUSED | space resume | Tab / 1-4 | q quit"
                                  : " Tab / 1-4 | " + std::string{controls[selected]} +
                                        " | space pause | q quit";
        hint.resize(std::min(hint.size(), std::size_t(std::max(0, area.width - 1))));
        append_encoded(bytes, ResetStyle{}, Cursor{0, area.height - 1},
                       bg(RGB{0x03050B}), BlankRow{}, fg(RGB{0x7893AA}), hint);
    }
    append_encoded(bytes, ResetStyle{});
    auto frame = terminal.frame();
    terminal.output().write(bytes);
    frame.close();
}
}  // namespace

int main()
{
    try {
        auto options = TerminalOptions::interactive();
        options.signals = Signals::Off;
        options.mouse = MouseMode::Move;
        options.profile = TerminalProfile{.control = true,
                                          .colors = ColorSupport::TrueColor,
                                          .alternate_screen = true,
                                          .cursor_visibility = true,
                                          .mouse_protocol = MouseProtocol::Sgr,
                                          .maximum_mouse_mode = MouseMode::Move};
        auto terminal = Terminal::open(options);
        (void)terminal.confirm_synchronized_output(100ms);
        std::array<Demo, 4> demos = {Liquid{}, Tide{}, Warp{}, Julia{}};
        unsigned selected = 0;
        Area area{};
        std::string bytes;
        bytes.reserve(256 * 1024);
        bool running = true, paused = false, dirty = true;
        auto last = Clock::now(), next = last;
        while (running) {
            auto size = terminal.area();
            if (size != area) {
                area = size;
                for (auto& demo : demos)
                    std::visit(
                        [&](auto& scene) {
                            scene.resize(std::max(0, area.width - 1),
                                         std::max(0, area.height - 2));
                        },
                        demo);
                terminal.output().write(ResetStyle{}, BlankScreen{});
                dirty = true;
            }
            auto now = Clock::now();
            if (now >= next) {
                std::visit(
                    [&](auto& scene) {
                        if (!paused)
                            scene.step(
                                std::chrono::duration<double>(now - last).count());
                        if (!paused || dirty)
                            render(terminal, scene, area, paused, selected, bytes);
                    },
                    demos[selected]);
                dirty = false;
                last = now;
                next = now + 16ms;
            }
            auto remaining =
                std::chrono::ceil<std::chrono::milliseconds>(next - Clock::now());
            for (unsigned input = 0; input < 128 && running; ++input) {
                auto result =
                    terminal.read(input == 0 ? std::max(0ms, remaining) : 0ms);
                if (std::holds_alternative<Timeout>(result)) break;
                if (std::holds_alternative<EndOfInput>(result)) {
                    running = false;
                    break;
                }
                if (auto event = std::get_if<Event>(&result)) {
                    match(
                        *event,
                        [&](KeyEvent const& key) {
                            if (key.action == KeyAction::Release) return;
                            if (key.matches(U'q') || key.matches(KeyCode::Escape) ||
                                key.matches(U'c', Modifiers{.ctrl = true}) ||
                                key.matches(U'\x03'))
                                running = false;
                            else if (key.action == KeyAction::Press) {
                                if (key.matches(KeyCode::Tab) ||
                                    key.matches(KeyCode::ArrowRight)) {
                                    selected = (selected + 1) % demos.size();
                                    dirty = true;
                                }
                                else if (key.matches(KeyCode::Tab,
                                                     Modifiers{.shift = true}) ||
                                         key.matches(KeyCode::ArrowLeft)) {
                                    selected =
                                        (selected + demos.size() - 1) % demos.size();
                                    dirty = true;
                                }
                                else {
                                    for (unsigned i = 0; i < demos.size(); ++i)
                                        if (key.matches(
                                                static_cast<char32_t>(U'1' + i))) {
                                            selected = i;
                                            dirty = true;
                                        }
                                    if (key.matches(U' ')) {
                                        paused = !paused;
                                        dirty = true;
                                    }
                                }
                            }
                        },
                        [&](MouseMove const& mouse) {
                            if (mouse.at.y > 0 && mouse.at.y < area.height - 1)
                                std::visit([&](auto& scene) { scene.aim(mouse.at); },
                                           demos[selected]);
                        },
                        [&](MousePress const& mouse) {
                            if (mouse.button != MouseButton::Left) return;
                            if (mouse.at.y == 0 && mouse.at.x >= 7 && mouse.at.x < 55) {
                                selected = static_cast<unsigned>((mouse.at.x - 7) / 12);
                                dirty = true;
                            }
                            else if (mouse.at.y > 0 && mouse.at.y < area.height - 1)
                                std::visit([&](auto& scene) { scene.splash(mouse.at); },
                                           demos[selected]);
                        },
                        [&](MouseWheel const& mouse) {
                            std::visit([&](auto& scene) { scene.shift(mouse.dy); },
                                       demos[selected]);
                            dirty = true;
                        },
                        [&](auto const&) {});
                }
            }
        }
        terminal.close();
        return 0;
    }
    catch (std::exception const& error) {
        std::cerr << "lumen: " << error.what() << '\n';
        return 1;
    }
}
