#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <optional>
#include <ranges>
#include <variant>

namespace mini::gl {

MultisegmentBezierCurveRS MultisegmentBezierCurveRS::create() {
  return {.control_points_ebo = eray::driver::gl::ElementBuffer::create(), .last_bezier_degree = 0};
}

BSplineCurveRS BSplineCurveRS::create() {
  auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  auto vao = eray::driver::gl::VertexArray::create(eray::driver::gl::VertexBuffer::create(std::move(vbo_layout)),
                                                   eray::driver::gl::ElementBuffer::create());
  return {
      .bernstein_points_vao = std::move(vao),
      .de_boor_points_ebo   = eray::driver::gl::ElementBuffer::create(),
  };
}

NaturalSplineCurveRS NaturalSplineCurveRS::create() {
  auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  auto vao = eray::driver::gl::VertexArray::create(eray::driver::gl::VertexBuffer::create(std::move(vbo_layout)),
                                                   eray::driver::gl::ElementBuffer::create());

  return {.coefficients_vao = std::move(vao), .coefficients_buffer = std::vector<float>()};
}

void NaturalSplineCurveRS::reset_buffer(const PointListObject& obj) {
  auto write_point = [&](size_t index, const auto& point) {
    coefficients_buffer[3 * index]     = point.x;
    coefficients_buffer[3 * index + 1] = point.y;
    coefficients_buffer[3 * index + 2] = point.z;
  };

  std::visit(eray::util::match{//
                               [&](const NaturalSplineCurve& nsc) {
                                 coefficients_buffer.resize(nsc.segments().size() * 6 * 3, 0.F);
                                 for (const auto& [i, segment] : nsc.segments() | std::views::enumerate) {
                                   size_t idx = static_cast<size_t>(i) * 6;

                                   write_point(idx, segment.a);
                                   write_point(idx + 1, segment.b);
                                   write_point(idx + 2, segment.c);
                                   write_point(idx + 3, segment.d);
                                 }
                                 for (const auto& [i, points] :
                                      nsc.used_points() | std::views::adjacent<2> | std::views::enumerate) {
                                   size_t idx           = static_cast<size_t>(i) * 6;
                                   const auto& [p0, p1] = points;

                                   write_point(idx + 4, p0);
                                   write_point(idx + 5, p1);
                                 }
                                 coefficients_vao.vbo().buffer_data(std::span{coefficients_buffer},
                                                                    eray::driver::gl::DataUsage::StaticDraw);
                               },
                               [](const auto&) {
                                 eray::util::Logger::warn(
                                     "Requested NaturalSplineCurveRS update, but the PointListObject does not store "
                                     "the NaturalSplineCurve.");
                               }},
             obj.object);
}

PointListObjectRS PointListObjectRS::create(const eray::driver::gl::VertexBuffer& points_vao,
                                            const PointListObject& point_list_obj) {
  GLuint id = 0;
  glCreateVertexArrays(1, &id);

  auto ebo = eray::driver::gl::ElementBuffer::create();

  // Bind EBO to VAO
  glVertexArrayElementBuffer(id, ebo.raw_gl_id());

  // Apply layouts of VBO
  GLsizei vertex_size = 0;

  for (const auto& attrib : points_vao.layout()) {
    glEnableVertexArrayAttrib(id, attrib.location);
    glVertexArrayAttribFormat(id,                                     //
                              attrib.location,                        //
                              static_cast<GLint>(attrib.count),       //
                              GL_FLOAT,                               //
                              attrib.normalize ? GL_TRUE : GL_FALSE,  //
                              vertex_size);                           //

    vertex_size += static_cast<GLint>(sizeof(float) * attrib.count);
    glVertexArrayAttribBinding(id, attrib.location, 0);
  }

  glVertexArrayVertexBuffer(id, 0, points_vao.raw_gl_id(), 0, vertex_size);

  eray::driver::gl::check_gl_errors();

  return {
      .state          = ::mini::PointListObjectRS(VisibilityState::Visible),
      .vao            = eray::driver::gl::VertexArrayHandle(id),
      .polyline_ebo   = std::move(ebo),
      .specialized_rs = std::visit(eray::util::match{
                                       [](MultisegmentBezierCurve&) { return MultisegmentBezierCurveRS::create(); },
                                       [](auto&) { return std::nullopt; },
                                   },
                                   point_list_obj.object),
  };
}

SceneObjectRS SceneObjectRS::create(const SceneObject& scene_obj) {
  auto specialized_rs = std::visit(
      eray::util::match{
          [](const Point&) -> std::variant<PointRS, SurfaceParametrizationRS> { return PointRS{}; },
          [](const Torus&) -> std::variant<PointRS, SurfaceParametrizationRS> { return SurfaceParametrizationRS{}; },
      },
      scene_obj.object);
  return {
      .state          = ::mini::SceneObjectRS(VisibilityState::Visible),
      .specialized_rs = specialized_rs,
  };
}

}  // namespace mini::gl
