#include "libminicad/renderer/rendering_state.hpp"

#include <libminicad/renderer/gl/rendering_state.hpp>
#include <optional>
#include <variant>

#include "liberay/driver/gl/buffer.hpp"
#include "liberay/util/variant_match.hpp"
#include "libminicad/renderer/visibility_state.hpp"
#include "libminicad/scene/scene_object.hpp"

namespace mini::gl {

MultisegmentBezierCurveRS MultisegmentBezierCurveRS::create() {
  return {.thrd_degree_bezier_ebo = eray::driver::gl::ElementBuffer::create(), .last_bezier_degree = 0};
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

SceneObjectRS SceneObjectRS::create(const SceneObject& /*scene_obj*/) {
  return {
      .state          = ::mini::SceneObjectRS(VisibilityState::Visible),
      .specialized_rs = PointRS{},
  };
}

}  // namespace mini::gl
