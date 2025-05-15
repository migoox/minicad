#pragma once

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini::gl {

struct BillboardRS {
  BillboardRS() = delete;
  explicit BillboardRS(eray::driver::gl::TextureHandle&& _texture) : texture(std::move(_texture)) {}

  ::mini::BillboardRS state;

  eray::driver::gl::TextureHandle texture;
};

}  // namespace mini::gl
