#ifndef ESC_TRAIT_HPP
#define ESC_TRAIT_HPP
#include <cstdint>

#include <esc/mask.hpp>

namespace esc {

/// Visual Traits that can be applied to terminal text with set_traits(...).
enum class Trait : std::uint16_t {
    Standout         = 1,
    Bold             = 2,
    Dim              = 4,
    Italic           = 8,
    Underline        = 16,
    Blink            = 32,
    Inverse          = 64,
    Invisible        = 128,
    Crossed_out      = 256,
    Double_underline = 512,
};

}  // namespace esc
#endif  // ESC_TRAIT_HPP
