#pragma once

/**
 * @file debug.hpp
 *
 * Debugging utilities for Escape.
 */

#include <iosfwd>
#include <string>

#include <esc/event.hpp>
#include <esc/key.hpp>
#include <esc/mouse.hpp>

namespace esc::detail {

/**
 * Translate a single non-visible character into a visible string.
 *
 * @details Visible chars pass through unchanged, into a string.
 * @param c The character to translate.
 * @return A visible string representation of the character.
 */
[[nodiscard]] auto display(char c) -> std::string;

/**
 * Translate any control characters to a visible byte string.
 *
 * @details Visible chars pass through unchanged.
 * @param sequence The string to translate.
 * @return A visible string representation of the sequence.
 */
[[nodiscard]] auto display(std::string const& sequence) -> std::string;

/**
 * Print KeyPress struct display to \p os.
 *
 * @param k The KeyPress struct to print.
 * @param os The output stream to print to.
 * @return The output stream \p os.
 */
auto print(::esc::KeyPress k, std::ostream& os) -> std::ostream&;

/**
 * Print MousePress struct display to \p os.
 *
 * @param m The MousePress struct to print.
 * @param os The output stream to print to.
 * @return The output stream \p os.
 */
auto print(::esc::MousePress m, std::ostream& os) -> std::ostream&;

/**
 * Print MouseRelease struct display to \p os.
 *
 * @param m The MouseRelease struct to print.
 * @param os The output stream to print to.
 * @return The output stream \p os.
 */
auto print(::esc::MouseRelease m, std::ostream& os) -> std::ostream&;

/**
 * Print MouseWheel struct display to \p os.
 *
 * @param m The MouseWheel struct to print.
 * @param os The output stream to print to.
 * @return The output stream \p os.
 */
auto print(::esc::MouseWheel m, std::ostream& os) -> std::ostream&;

/**
 * Print MouseMove struct display to \p os.
 *
 * @param m The MouseMove struct to print.
 * @param os The output stream to print to.
 * @return The output stream \p os.
 */
auto print(::esc::MouseMove m, std::ostream& os) -> std::ostream&;

/**
 * Print Resize struct display to \p os.
 *
 * @param w The Resize struct to print.
 * @param os The output stream to print to.
 * @return The output stream \p os.
 */
auto print(::esc::Resize w, std::ostream& os) -> std::ostream&;

}  // namespace esc::detail