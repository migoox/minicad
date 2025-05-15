#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/buffer.hpp>
#include <libminicad/renderer/gl/point_lists_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <variant>

namespace mini::gl {

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::Internal::AddObject&) {
  const auto& handle = cmd_ctx.handle;
  renderer.m_.curves.update_chunk(handle, obj.bezier3_points(), obj.bezier3_points_count());
  renderer.m_.polylines.update_chunk(handle, obj.polyline_points(), obj.polyline_points_count());
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::Internal::UpdateControlPoints&) {
  const auto& handle = cmd_ctx.handle;
  renderer.m_.curves.update_chunk(handle, obj.bezier3_points(), obj.bezier3_points_count());
  renderer.m_.polylines.update_chunk(handle, obj.polyline_points(), obj.polyline_points_count());
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::UpdateObjectVisibility& cmd) {
  if (obj_rs.visibility == cmd.new_visibility_state) {
    return;
  }
  const auto& handle = cmd_ctx.handle;
  renderer.m_.curves.delete_chunk(handle);
  renderer.m_.polylines.delete_chunk(handle);
  obj_rs.visibility = cmd.new_visibility_state;
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::ShowPolyline& cmd) {
  if (obj_rs.show_polyline == cmd.show) {
    return;
  }
  const auto& handle = cmd_ctx.handle;
  renderer.m_.polylines.update_chunk(handle, obj.polyline_points(), obj.polyline_points_count());
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::ShowBernsteinControlPoints&) {
  // TODO(migoox): update bernstein points visibility
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::UpdateBernsteinControlPoints&) {
  std::visit(eray::util::match{
                 [&](const BSplineCurve& curve) {
                   renderer.m_.helper_points.update_chunk(obj.handle(), curve.bezier3_points(obj),
                                                          curve.bezier3_points_count(obj));
                 },
                 [](const auto&) {},
             },
             obj.object);
}

PointListsRenderer PointListsRenderer::create() {
  auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  auto polylines_vao = eray::driver::gl::VertexArray::create(
      eray::driver::gl::VertexBuffer::create(eray::driver::gl::VertexBuffer::Layout(vbo_layout)),
      eray::driver::gl::ElementBuffer::create());
  auto helper_points_vao = eray::driver::gl::VertexArray::create(
      eray::driver::gl::VertexBuffer::create(eray::driver::gl::VertexBuffer::Layout(vbo_layout)),
      eray::driver::gl::ElementBuffer::create());
  auto curves_vao = eray::driver::gl::VertexArray::create(eray::driver::gl::VertexBuffer::create(std::move(vbo_layout)),
                                                          eray::driver::gl::ElementBuffer::create());

  return PointListsRenderer(Members{
      .helper_points_vao = std::move(helper_points_vao),
      .helper_points     = PointsChunksBuffer::create(),
      .polylines_vao     = std::move(polylines_vao),
      .polylines         = PointsChunksBuffer::create(),
      .curves_vao        = std::move(curves_vao),
      .curves            = PointsChunksBuffer::create(),
      .point_lists       = {},
      .cmds              = {},
  });
}

PointListsRenderer::PointListsRenderer(Members&& members) : m_(std::move(members)) {}

void PointListsRenderer::push_cmd(const PointListObjectRSCommand& cmd) { m_.cmds.push_back(cmd); }

void PointListsRenderer::update(Scene& scene) {
  // TODO(migoox): add commands preprocessing to remove repetitions

  // Flush the commands and invoke handler
  for (auto& cmd : m_.cmds) {
    auto add_obj    = std::holds_alternative<PointListObjectRSCommand::Internal::AddObject>(cmd.variant);
    auto delete_obj = std::holds_alternative<PointListObjectRSCommand::Internal::DeleteObject>(cmd.variant);

    if (add_obj) {
      m_.point_lists.emplace(cmd.handle, mini::PointListObjectRS(VisibilityState::Visible));
    }
    if (delete_obj) {
      m_.polylines.delete_chunk(cmd.handle);
      m_.curves.delete_chunk(cmd.handle);
      m_.helper_points.delete_chunk(cmd.handle);
    }
    if (!m_.point_lists.contains(cmd.handle)) {
      continue;
    }

    if (auto obj = scene.get_obj(cmd.handle)) {
      auto handler = PointListObjectRSCommandHandler(cmd, *this, *obj.value(), m_.point_lists.at(cmd.handle));
      std::visit(handler, cmd.variant);
    } else {
      m_.point_lists.erase(cmd.handle);
    }
  }
  m_.cmds.clear();

  // Sync the buffers
  m_.polylines.sync(m_.polylines_vao.vbo().handle());
  m_.curves.sync(m_.curves_vao.vbo().handle());
  m_.helper_points.sync(m_.helper_points_vao.vbo().handle());
}

void PointListsRenderer::render_polylines() const {
  m_.polylines_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_.polylines.chunks_count())));
}

void PointListsRenderer::render_curves() const {
  ERAY_GL_CALL(glPatchParameteri(GL_PATCH_VERTICES, 4));
  m_.curves_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_PATCHES, 0, static_cast<GLsizei>(m_.curves.chunks_count())));
}

void PointListsRenderer::render_helper_points() const {
  m_.helper_points_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_.helper_points.chunks_count())));
}

std::optional<PointListObjectRS> PointListsRenderer::object_rs(const PointListObjectHandle& handle) {
  if (!m_.point_lists.contains(handle)) {
    return std::nullopt;
  }

  return m_.point_lists.at(handle);
}

void PointListsRenderer::set_object_rs(const PointListObjectHandle& handle, const ::mini::PointListObjectRS& state) {
  if (m_.point_lists.contains(handle)) {
    m_.point_lists.at(handle) = state;
  }
}

}  // namespace mini::gl
