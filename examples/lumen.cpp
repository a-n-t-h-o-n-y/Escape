#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <esc/esc.hpp>
#include <iostream>
#include <numbers>
#include <string>
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

class Scene {
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

void render(Terminal& terminal,
            Scene const& scene,
            Area area,
            bool paused,
            std::string& bytes)
{
    bytes.clear();
    auto title = std::string{" LUMEN"};
    title.resize(std::min(title.size(), std::size_t(std::max(0, area.width - 1))));
    append_encoded(bytes, ResetStyle{}, Cursor{}, bg(RGB{0x03050B}), BlankRow{},
                   fg(RGB{0x9EDBFF}), title);
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
        std::string hint =
            paused ? " PAUSED | space resume | q quit"
                   : " mouse move | click ripple | wheel color | space pause | q quit";
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
        Scene scene;
        Area area{};
        std::string bytes;
        bytes.reserve(256 * 1024);
        bool running = true, paused = false, dirty = true;
        auto last = Clock::now(), next = last;
        while (running) {
            auto size = terminal.area();
            if (size != area) {
                area = size;
                scene.resize(std::max(0, area.width - 1), std::max(0, area.height - 2));
                terminal.output().write(ResetStyle{}, BlankScreen{});
                dirty = true;
            }
            auto now = Clock::now();
            if (now >= next) {
                if (!paused)
                    scene.step(std::chrono::duration<double>(now - last).count());
                if (!paused || dirty) render(terminal, scene, area, paused, bytes);
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
                            else if (key.matches(U' ') &&
                                     key.action == KeyAction::Press) {
                                paused = !paused;
                                dirty = true;
                            }
                        },
                        [&](MouseMove const& mouse) { scene.aim(mouse.at); },
                        [&](MousePress const& mouse) { scene.splash(mouse.at); },
                        [&](MouseWheel const& mouse) {
                            scene.shift(mouse.dy);
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
