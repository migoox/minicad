#pragma once

#include <liberay/driver/gl/buffer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <variant>

namespace mini::gl {

//----------------------------------------------------------------------------------------------------------------------

struct BillboardRS {
  BillboardRS() = delete;
  explicit BillboardRS(eray::driver::gl::TextureHandle&& _texture) : texture(std::move(_texture)) {}

  ::mini::BillboardRS state;

  eray::driver::gl::TextureHandle texture;
};

//----------------------------------------------------------------------------------------------------------------------

struct MultisegmentBezierCurveRS {
  static constexpr int kMaxBezierDegree    = 3;
  static constexpr size_t kMaxBezierPoints = 4;

  static MultisegmentBezierCurveRS create();

  eray::driver::gl::ElementBuffer thrd_degree_bezier_ebo;
  int last_bezier_degree;
};

struct PointListObjectRS {
  static PointListObjectRS create(const eray::driver::gl::VertexBuffer& points_vao,
                                  const PointListObject& point_list_obj);

  ::mini::PointListObjectRS state;

  eray::driver::gl::VertexArrayHandle vao;
  eray::driver::gl::ElementBuffer polyline_ebo;

  std::optional<std::variant<MultisegmentBezierCurveRS>> specialized_rs;
};

//----------------------------------------------------------------------------------------------------------------------

struct PointRS {};

struct TorusRS {};

struct SceneObjectRS {
  static SceneObjectRS create(const SceneObject& scene_obj);
  ::mini::SceneObjectRS state;

  std::variant<PointRS, TorusRS> specialized_rs;
};

//----------------------------------------------------------------------------------------------------------------------

}  // namespace mini::gl
