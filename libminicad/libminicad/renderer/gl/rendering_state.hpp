#pragma once

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
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

struct NaturalSplineCurveRS {
  static constexpr int kMaxBSplineDegree    = 3;
  static constexpr size_t kMaxBSplinePoints = kMaxBSplineDegree + 1;

  static NaturalSplineCurveRS create();

  /**
   * @brief Fills the GPU coefficients vbo basing on the provided point list object
   *
   * @param obj
   */
  void reset_buffer(const PointListObject& obj);

  eray::driver::gl::VertexArray coefficients_vao;
  std::vector<float> coefficients_buffer;
};

struct PointListObjectRS {
  static PointListObjectRS create(const eray::driver::gl::VertexBuffer& points_vao,
                                  const PointListObject& point_list_obj);

  ::mini::PointListObjectRS state;

  eray::driver::gl::VertexArrayHandle vao;
  eray::driver::gl::ElementBuffer polyline_ebo;

  std::optional<std::variant<MultisegmentBezierCurveRS, BSplineCurveRS, NaturalSplineCurveRS>> specialized_rs;
};

//----------------------------------------------------------------------------------------------------------------------

struct PointRS {};

struct SurfaceParametrizationRS {};

struct SceneObjectRS {
  static SceneObjectRS create(const SceneObject& scene_obj);
  ::mini::SceneObjectRS state;

  std::variant<PointRS, SurfaceParametrizationRS> specialized_rs;
};

//----------------------------------------------------------------------------------------------------------------------

}  // namespace mini::gl
