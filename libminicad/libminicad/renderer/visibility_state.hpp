#pragma once

#include <cstdint>

namespace mini {

enum class VisibilityState : uint8_t {
  Visible   = 0,
  Selected  = 1,
  Invisible = 2,
};

}
