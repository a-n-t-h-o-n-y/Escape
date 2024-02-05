#pragma once

#include <string>

#include <esc/trait.hpp>

namespace esc::detail {

/// Translate a single Trait into its string control sequence parameter integer.
[[nodiscard]] auto trait_to_int_sequence(Trait t) -> std::string;

/// Turn a mask of traits into a string control sequence of parameter integers.
[[nodiscard]] auto traits_to_int_sequence(Traits traits) -> std::string;

}  // namespace esc::detail