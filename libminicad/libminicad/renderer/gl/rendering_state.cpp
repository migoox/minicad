#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <optional>
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
