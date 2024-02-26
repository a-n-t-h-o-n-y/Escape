#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <variant>
#include <vector>

#include <esc/esc.hpp>

using namespace esc;

auto screen_dimensions = Area{.width = 0, .height = 0};

constexpr auto logo = std::string_view{
    R"('||''|.   ..|''||   '|.   '|'  ..|'''.|
 ||   || .|'    ||   |'|   |  .|'     '
 ||...|' ||      ||  | '|. |  ||    ....
 ||      '|.     ||  |   |||  '|.    ||
.||.      ''|...|'  .|.   '|   ''|...'|
)"};

// This is a std::variant based state machine, one struct per screen type with
// get_display_bytes() to retrieve the display for each state type and process()
// to handle events for each state.

// STATES ----------------------------------------------------------------------

struct SplashScreen {
    std::uint16_t hue{0};
    std::string_view display = logo;
};

struct Menu {
    std::vector<std::string> options{
        "Play",
        "How-To",
        "Quit",
    };
    std::size_t selected{0};
};

struct Game {
    static constexpr auto game_space = Area{.width = 81, .height = 25};

    struct AI {
        int action_interval;
        float reaction_threshold;
        float velocity;
    };

    struct Player {
        std::string name;
        std::optional<AI> ai;
    };

    struct Location {
        float x;
        float y;
    };

    struct Paddle {
        Location top;
        float dy;
        static constexpr auto height = 4.f;
    };

    struct {
        std::size_t score = 0;
        Player player;
        Paddle paddle{
            .top = {.x = 1, .y = (game_space.height - Paddle::height) / 2},
            .dy  = 0,
        };
    } left;

    struct {
        std::size_t score = 0;
        Player player;
        Paddle paddle{
            .top = {.x = game_space.width - 2,
                    .y = (game_space.height - Paddle::height) / 2},
            .dy  = 0,
        };
    } right;

    struct Ball {
        Location at = {.x = game_space.width / 2, .y = game_space.height / 2};
        struct Velocity {
            float dx = 0;
            float dy = 0;
        } velocity;
    } ball{};
};

struct PlayerSelectMenu {
    std::string title{"Select Players"};

    Game::Player left_player = {
        .name = "Human",
        .ai   = std::nullopt,
    };
    Game::Player right_player = {
        .name = "Human",
        .ai   = std::nullopt,
    };

    bool left_selected{true};

    std::vector<Game::Player> options{
        {
            "Slow AI",
            Game::AI{
                .action_interval    = 7,
                .reaction_threshold = 60.f,
                .velocity           = 0.1f,
            },
        },
        {
            "Medium AI",
            Game::AI{
                .action_interval    = 5,
                .reaction_threshold = 30.f,
                .velocity           = 0.2f,
            },
        },
        {
            "Fast AI",
            Game::AI{
                .action_interval    = 3,
                .reaction_threshold = 25.f,
                .velocity           = 0.25f,
            },
        },
        {
            "Aggressive AI",
            Game::AI{
                .action_interval    = 1,
                .reaction_threshold = 10.f,
                .velocity           = 0.75f,
            },
        },
        {
            "Human",
            std::nullopt,
        },
    };
    std::size_t selected{0};
};

struct HowTo {
    std::string title{"How to Play"};
    std::vector<std::array<std::string, 2>> instructions{
        {"Up/Down Arrow Keys", "Move the right paddle"},
        {"w/s Keys", "Move the left paddle"},
        {"Esc Key", "Return to the main menu"},
        {"Enter Key", "Start next round"},
    };
};

using State = std::variant<SplashScreen, Menu, Game, PlayerSelectMenu, HowTo>;

// EVENTS ----------------------------------------------------------------------

// Event handlers return the next State.
// std::nullopt is a quit request
using EventResponse = std::optional<State>;

[[nodiscard]] auto process(KeyPress event, SplashScreen state) -> EventResponse
{
    switch (event.key) {
        case Key::Enter: return Menu{};
        default: return state;
    }
}

[[nodiscard]] auto process(KeyPress event, Menu state) -> EventResponse
{
    switch (event.key) {
        case Key::ArrowUp:
        case Key::k: {
            state.selected = (state.selected + state.options.size() - 1) %
                             state.options.size();
            return state;
        }

        case Key::ArrowDown:
        case Key::j: {
            state.selected = (state.selected + 1) % state.options.size();
            return state;
        }

        case Key::Enter:
            switch (state.selected) {
                case 0: return PlayerSelectMenu{};
                case 1: return HowTo{};
                case 2: return std::nullopt;
                default: return state;
            }

        default: return state;
    }
}

[[nodiscard]] auto generate_initial_velocity() -> Game::Ball::Velocity
{
    using int_dist   = std::uniform_int_distribution<int>;
    using float_dist = std::uniform_real_distribution<float>;

    auto rng = std::mt19937{std::random_device{}()};
    auto dx  = float_dist{0.5, 0.8}(rng);
    auto dy  = float_dist{0.1, 0.4}(rng);
    if (int_dist{0, 1}(rng) == 0) {
        dx = -dx;
    }
    if (int_dist{0, 1}(rng) == 0) {
        dy = -dy;
    }
    return {dx, dy};
}

[[nodiscard]] auto process(KeyPress event, Game state) -> EventResponse
{
    constexpr auto speed_increment = 0.5f;

    switch (event.key) {
        case Key::Escape: return Menu{};

        case Key::Enter: {
            auto& ball = state.ball;
            if (ball.velocity.dx == 0 && ball.velocity.dy == 0) {
                ball.velocity = generate_initial_velocity();
            }
            return state;
        }

        case Key::ArrowUp: {
            state.right.paddle.dy -= speed_increment;
            return state;
        }
        case Key::ArrowDown: {
            state.right.paddle.dy += speed_increment;
            return state;
        }

        case Key::w: {
            state.left.paddle.dy -= speed_increment;
            return state;
        }
        case Key::s: {
            state.left.paddle.dy += speed_increment;
            return state;
        }

        default: return state;
    }
}

[[nodiscard]] auto process(KeyPress event, PlayerSelectMenu state)
    -> EventResponse
{
    switch (event.key) {
        case Key::Escape: return Menu{};

        case Key::ArrowUp:
        case Key::k: {
            state.selected = (state.selected + state.options.size() - 1) %
                             state.options.size();
            return state;
        }

        case Key::ArrowDown:
        case Key::j: {
            state.selected = (state.selected + 1) % state.options.size();
            return state;
        }

        case Key::Enter: {
            if (state.left_selected) {
                state.left_player   = state.options[state.selected];
                state.left_selected = false;
                return state;
            }
            else {
                state.right_player = state.options[state.selected];
                return Game{
                    .left  = {.player = state.left_player},
                    .right = {.player = state.right_player},
                };
            }
        }
        default: return state;
    }
}

[[nodiscard]] auto process(KeyPress event, HowTo state) -> EventResponse
{
    switch (event.key) {
        case Key::Escape: return Menu{};
        default: return state;
    }
}

[[nodiscard]] auto process(Resize event, auto state) -> EventResponse
{
    screen_dimensions = event.size;
    return state;
}

// Catch-All
[[nodiscard]] auto process(auto, auto state) -> EventResponse { return state; }

// DISPLAY ---------------------------------------------------------------------

[[nodiscard]] auto get_display_bytes(SplashScreen const& state, Area dimensions)
    -> std::string
{
    std::uint16_t hue = state.hue;

    auto const padding = [&] {
        auto const width = (int)state.display.find('\n');
        return std::string(std::max((dimensions.width - width) / 2, 0), ' ');
    }();

    auto hsl = HSL{.hue = hue, .saturation = 90, .lightness = 70};

    auto bytes = escape(Cursor{.x = 0, .y = 6}, Trait::Bold);
    bytes += escape(fg(TColor{hsl})) + padding;
    for (char c : state.display) {
        bytes += c;
        if (c == '\n') {
            hsl.hue = (hsl.hue + 20) % 360;
            bytes += escape(fg(TColor{hsl})) + padding;
        }
    }

    bytes += escape(Brush{});

    auto const enter = std::string{"Press Enter to continue"};
    bytes += escape(Cursor{
        .x = (screen_dimensions.width - (int)enter.size()) / 2,
        .y = screen_dimensions.height - 2,
    });
    return bytes += escape(Trait::Dim) + enter + escape(Trait::None);
}

[[nodiscard]] auto get_display_bytes(Menu const& state, Area dimensions)
    -> std::string
{
    auto const padding = [&] {
        auto const width = (int)logo.find('\n');
        return std::string((dimensions.width - width) / 2, ' ');
    }();

    auto bytes = escape(Cursor{.x = 0, .y = 6});
    bytes += padding;
    for (char c : logo) {
        bytes += c;
        if (c == '\n') {
            bytes += padding;
        }
    }

    for (std::size_t i = 0; i < state.options.size(); ++i) {
        bytes += escape(Cursor{
            .x = (dimensions.width - (int)state.options[i].size()) / 2,
            .y = 14 + (int)i,
        });
        if (i == state.selected) {
            bytes += escape(Trait::Standout);
        }
        bytes += state.options[i] + escape(Trait::None);
    }

    return bytes;
}

[[nodiscard]] auto velocity_to_color(Game::Ball::Velocity velocity) -> Color
{
    auto const speed =
        std::sqrt(velocity.dx * velocity.dx + velocity.dy * velocity.dy);

    return TColor{HSL{
        .hue        = std::uint16_t((90 + (int)(speed * 160)) % 360),
        .saturation = 80,
        .lightness  = 70,
    }};
}

[[nodiscard]] auto get_display_bytes(Game const& state, Area dimensions)
    -> std::string
{
    static constexpr auto display_space = Game::game_space;

    if (dimensions.width < display_space.width ||
        dimensions.height < display_space.height) {
        return escape(Cursor{.x = 0, .y = 0}) +
               "Terminal too small to display game";
    }

    auto const offset = Point{
        .x = (dimensions.width - display_space.width) / 2,
        .y = (dimensions.height - display_space.height) / 2,
    };

    auto bytes = std::string{};

    {  // Net
        bytes += escape(Trait::Dim);
        for (int i = 0; i < display_space.height; ++i) {
            bytes += escape(offset + Point{display_space.width / 2, i}) + "╳";
        }
        bytes += escape(Trait::None);
    }

    {  // Ball
        auto const glyph = [&] {
            auto const y = state.ball.at.y;
            auto const i = (std::size_t)((y - std::floor(y)) * 8);
            return std::array{"▄", "▃", "▂", "▁", "█", "▇", "▆", "▅"}[i];
        }();

        auto cursor = offset + Point{.x = (int)std::round(state.ball.at.x),
                                     .y = (int)std::round(state.ball.at.y)};

        bytes += escape(fg(velocity_to_color(state.ball.velocity)));
        bytes += escape(cursor, Trait::Inverse) + glyph;
        cursor.y -= 1;
        bytes += escape(cursor, Trait::None) + glyph;
        bytes += escape(Brush{});
    }

    {  // Paddles
        auto const paint_paddle = [&](Game::Paddle const& paddle) {
            auto cursor = Cursor{
                .x = offset.x + (int)std::floor(paddle.top.x),
                .y = offset.y + (int)std::floor(paddle.top.y),
            };

            auto bytes = escape(cursor);

            auto const edge = [&] {
                auto const y = paddle.top.y;
                auto const i = (std::size_t)((y - std::floor(y)) * 8);
                return std::array{"█", "▇", "▆", "▅", "▄", "▃", "▂", "▁"}[i];
            }();

            bytes += edge;
            cursor.y += 1;

            for (int i = 1; i < Game::Paddle::height; ++i) {
                bytes += escape(cursor) + "█";
                cursor.y += 1;
            }

            return bytes +=
                   escape(cursor, Trait::Inverse) + edge + escape(Trait::None);
        };

        bytes += paint_paddle(state.left.paddle);
        bytes += paint_paddle(state.right.paddle);
    }

    {  // Scores
        bytes += escape(offset - Point{.x = 0, .y = 2});
        bytes += state.left.player.name + ": ";
        bytes += std::to_string(state.left.score);

        auto const rhs = state.right.player.name + ": ";
        bytes +=
            escape(offset + Point{
                                .x = display_space.width - (int)rhs.size() - 1,
                                .y = -2,
                            });
        bytes += rhs;
        bytes += std::to_string(state.right.score);
    }

    {  // Border
        bytes += escape(offset - Point{.x = 1, .y = 1}) +
                 escape(fg(XColor::Blue), Trait::Dim);
        bytes += "╭";

        for (int i = 0; i < display_space.width; ++i) {
            bytes += "─";
        }
        bytes += "╮";

        for (int i = 0; i < display_space.height; ++i) {
            bytes += escape(offset + Point{.x = -1, .y = i});
            bytes += "│";

            bytes += escape(offset + Point{.x = display_space.width, .y = i});
            bytes += "│";
        }

        bytes += escape(offset + Point{.x = -1, .y = display_space.height});
        bytes += "╰";

        for (int i = 0; i < display_space.width; ++i) {
            bytes += "─";
        }
        bytes += "╯" + escape(Brush{});
    }

    bytes += escape(Cursor{.x = 0, .y = dimensions.height - 1});
    bytes += escape(Trait::Dim) + "Press Esc to return to the main menu";

    auto const enter = std::string{"Press Enter to start next round"};
    bytes += escape(Cursor{
        .x = dimensions.width - (int)enter.size(),
        .y = dimensions.height - 1,
    });
    bytes += enter + escape(Trait::None);

    return bytes;
}

[[nodiscard]] auto get_display_bytes(PlayerSelectMenu const& state,
                                     Area dimensions) -> std::string
{
    auto bytes = escape(Cursor{
        .x = (dimensions.width - (int)state.title.size()) / 2,
        .y = 2,
    });

    bytes += escape(Trait::Bold | Trait::Underline);
    bytes += state.title;
    bytes += escape(Trait::None);

    bytes += escape(Cursor{.x = (dimensions.width) / 2 - 25, .y = 4});
    bytes += "Left Player: ";
    if (state.left_selected) {
        bytes += escape(Trait::Standout);
    }
    bytes += state.left_player.name;
    bytes += escape(Trait::None);

    bytes += escape(Cursor{.x = (dimensions.width) / 2 + 10, .y = 4});
    bytes += "Right Player: ";
    if (!state.left_selected) {
        bytes += escape(Trait::Standout);
    }
    bytes += state.right_player.name;
    bytes += escape(Trait::None);

    for (std::size_t i = 0; i < state.options.size(); ++i) {
        bytes += escape(Cursor{
            .x = (dimensions.width - (int)state.options[i].name.size()) / 2,
            .y = 6 + (int)i,
        });
        if (i == state.selected) {
            bytes += escape(Trait::Standout);
        }
        bytes += state.options[i].name;
        bytes += escape(Trait::None);
    }

    bytes += escape(Cursor{.x = 0, .y = dimensions.height - 1});
    bytes += escape(Trait::Dim) + "Press Esc to return to the main menu" +
             escape(Trait::None);

    return bytes;
}

[[nodiscard]] auto get_display_bytes(HowTo const& state, Area dimensions)
    -> std::string
{
    auto bytes = escape(Cursor{
        .x = (dimensions.width - (int)state.title.size()) / 2,
        .y = 2,
    });

    bytes += escape(Trait::Bold | Trait::Underline) + state.title +
             escape(Trait::None);

    for (std::size_t i = 0; i < state.instructions.size(); ++i) {
        bytes += escape(Cursor{
            .x = (dimensions.width - 40) / 2,
            .y = 4 + (int)i,
        });
        bytes += escape(Trait::Bold) + state.instructions[i][0] +
                 escape(Trait::None);
        bytes += escape(Cursor{
            .x = (dimensions.width + 8) / 2,
            .y = 4 + (int)i,
        });
        bytes += state.instructions[i][1];
    }

    // Border
    bytes += escape(Cursor{.x = 0, .y = 0}, Trait::Dim);
    bytes += "╭";
    for (int i = 0; i < dimensions.width - 2; ++i) {
        bytes += "─";
    }
    bytes += "╮";
    for (int i = 0; i < dimensions.height - 2; ++i) {
        bytes += escape(Cursor{.x = 0, .y = i + 1});
        bytes += "│";
        bytes += escape(Cursor{.x = dimensions.width - 1, .y = i + 1});
        bytes += "│";
    }
    bytes += escape(Cursor{.x = 0, .y = dimensions.height - 1});
    bytes += "╰";
    for (int i = 0; i < dimensions.width - 2; ++i) {
        bytes += "─";
    }
    bytes += "╯";
    bytes += escape(Trait::None);

    // Press Esc
    bytes += escape(Cursor{.x = 2, .y = dimensions.height - 1});
    bytes += escape(Trait::Dim) + "Press Esc to return to the main menu" +
             escape(Trait::None);

    return bytes;
}

// INCREMENT -------------------------------------------------------------------

[[nodiscard]] auto increment_game_state(Game state) -> std::optional<State>
{
    auto& ball  = state.ball;
    auto& left  = state.left;
    auto& right = state.right;

    // paddle/ball collisions
    if (ball.velocity.dx < 0.f && ball.at.x < 2.f &&
        ball.at.y + 0.5f >= left.paddle.top.y &&
        ball.at.y - 0.5f <= left.paddle.top.y + Game::Paddle::height) {
        ball.velocity.dx *= -1.07;
        ball.velocity.dy += left.paddle.dy * 0.25f;
        ball.at.x = 2.f;
    }
    else if (ball.velocity.dx > 0.f &&
             ball.at.x > Game::game_space.width - 3.f &&
             ball.at.y + 0.5f >= right.paddle.top.y &&
             ball.at.y - 0.5f <= right.paddle.top.y + Game::Paddle::height) {
        ball.velocity.dx *= -1.07;
        ball.velocity.dy += right.paddle.dy * 0.25f;
        ball.at.x = Game::game_space.width - 3.f;
    }

    // move paddles
    left.paddle.top.y += left.paddle.dy;
    right.paddle.top.y += right.paddle.dy;

    // move ball
    ball.at.x += ball.velocity.dx;
    ball.at.y += ball.velocity.dy;

    // slow down paddles
    left.paddle.dy *= 0.9;
    right.paddle.dy *= 0.9;

    // slow down ball
    ball.velocity.dx *= 0.9999;
    ball.velocity.dy *= 0.999;

    // check for ball/wall collisions
    if (ball.at.y <= 0.5) {
        ball.velocity.dy = -ball.velocity.dy;
        ball.at.y        = 0.51;
    }
    else if (ball.at.y >= Game::game_space.height - 0.5) {
        ball.velocity.dy = -ball.velocity.dy;
        ball.at.y        = Game::game_space.height - 0.51;
    }

    constexpr auto init_ball = Game::Ball{
        .at       = {.x = Game::game_space.width / 2,
                     .y = Game::game_space.height / 2},
        .velocity = {0, 0},
    };

    // check for score and reset
    if (ball.at.x < 0) {
        right.score++;
        ball = init_ball;
    }
    else if (ball.at.x > Game::game_space.width - 1) {
        left.score++;
        ball = init_ball;
    }

    // check for paddle/wall collisions
    if (left.paddle.top.y < 0) {
        left.paddle = {
            .top = {.x = 1, .y = 0},
            .dy  = 0,
        };
    }
    else if (left.paddle.top.y >
             Game::game_space.height - Game::Paddle::height) {
        left.paddle = {
            .top = {.x = 1,
                    .y = Game::game_space.height - Game::Paddle::height},
            .dy  = 0,
        };
    }

    if (right.paddle.top.y < 0) {
        right.paddle = {
            .top = {.x = Game::game_space.width - 2, .y = 0},
            .dy  = 0,
        };
    }
    else if (right.paddle.top.y >
             Game::game_space.height - Game::Paddle::height) {
        right.paddle = {
            .top = {.x = Game::game_space.width - 2,
                    .y = Game::game_space.height - Game::Paddle::height},
            .dy  = 0,
        };
    }

    // AI Left Player
    if (left.player.ai.has_value()) {
        static auto frame_count = 0;

        auto& ai     = *left.player.ai;
        auto& paddle = left.paddle;

        if (ball.velocity.dx < 0.f && ball.at.x < ai.reaction_threshold) {
            if (frame_count % ai.action_interval == 0) {
                auto const paddle_center = paddle.top.y + paddle.height / 2.f;
                auto const distance      = ball.at.y - paddle_center;
                paddle.dy += ai.velocity * std::min(distance, 5.f) /
                             ((ball.at.x / ai.reaction_threshold) + 1);
            }
            ++frame_count;
        }
        else {
            frame_count = 0;
        }
    }

    // AI Right Player
    if (right.player.ai.has_value()) {
        static auto frame_count = 0;

        auto& ai     = *right.player.ai;
        auto& paddle = right.paddle;

        if (ball.velocity.dx > 0.f &&
            ball.at.x > Game::game_space.width - ai.reaction_threshold) {
            if (frame_count % ai.action_interval == 0) {
                auto const paddle_center = paddle.top.y + paddle.height / 2.f;
                auto const distance      = ball.at.y - paddle_center;
                paddle.dy += ai.velocity * std::min(distance, 5.f) /
                             (((Game::game_space.width - ball.at.x) /
                               ai.reaction_threshold) +
                              1);
            }
            ++frame_count;
        }
        else {
            frame_count = 0;
        }
    }

    return state;
}

// MAIN ------------------------------------------------------------------------

auto try_main() -> int
{
    constexpr auto init_timeout = std::chrono::milliseconds{30};

    initialize_interactive_terminal();
    screen_dimensions = terminal_area();

    std::optional<State> state = SplashScreen{};

    auto timeout = init_timeout;

    while (state.has_value() && sigint_flag == 0) {
        write(escape(BlankScreen{}));
        write(std::visit(
            [](auto const& state) {
                return get_display_bytes(state, screen_dimensions);
            },
            *state));
        flush();

        auto const begin = std::chrono::steady_clock::now();
        if (auto const event = read(timeout.count()); event.has_value()) {
            state = std::visit(
                [](auto event, auto current_state) {
                    return process(event, std::move(current_state));
                },
                *event, std::move(*state));
            timeout -= std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin);
        }
        else {  // Event Timeout
            // state is guaranteed to not be std::nullopt here.
            if (std::holds_alternative<Game>(*state)) {
                state = increment_game_state(std::get<Game>(*state));
            }
            else if (std::holds_alternative<SplashScreen>(*state)) {
                auto& splash = std::get<SplashScreen>(*state);
                splash.hue   = (splash.hue + 1) % 360;
                state        = splash;
            }
            timeout = init_timeout;
        }
    }

    uninitialize_terminal();
    return 0;
}

auto main() -> int
{
    try {
        return try_main();
    }
    catch (const std::exception& e) {
        uninitialize_terminal();
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        uninitialize_terminal();
        std::cerr << "Unknown error" << std::endl;
        return 1;
    }
}