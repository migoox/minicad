#pragma once

#include <liberay/driver/gl/buffer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <variant>

#include "liberay/driver/gl/gl_handle.hpp"
#include "liberay/driver/gl/vertex_array.hpp"

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
  static constexpr size_t kMaxBezierPoints = kMaxBezierDegree + 1;

  static MultisegmentBezierCurveRS create();

  eray::driver::gl::ElementBuffer control_points_ebo;
  int last_bezier_degree;
};

struct BSplineCurveRS {
  static constexpr int kMaxBSplineDegree    = 3;
  static constexpr size_t kMaxBSplinePoints = kMaxBSplineDegree + 1;

  static BSplineCurveRS create();

  eray::driver::gl::VertexArray bernstein_points_vao;
  eray::driver::gl::ElementBuffer de_boor_points_ebo;
};

struct PointListObjectRS {
  static PointListObjectRS create(const eray::driver::gl::VertexBuffer& points_vao,
                                  const PointListObject& point_list_obj);

  ::mini::PointListObjectRS state;

  eray::driver::gl::VertexArrayHandle vao;
  eray::driver::gl::ElementBuffer polyline_ebo;

  std::optional<std::variant<MultisegmentBezierCurveRS, BSplineCurveRS>> specialized_rs;
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
