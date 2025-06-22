#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/buffer.hpp>
#include <libminicad/renderer/gl/curves_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <variant>

namespace mini::gl {

namespace util = eray::util;

void CurveRSCommandHandler::operator()(const CurveRSCommand::Internal::AddObject&) {
  const auto& handle = cmd_ctx.handle;

  renderer.rs_.emplace(handle, mini::CurveRS(VisibilityState::Visible));

  if (auto o = scene.arena<Curve>().get_obj(handle)) {
    auto& obj = *o.value();

    renderer.m_.curves.update_chunk(handle, obj.bezier3_points());
    renderer.m_.polylines.update_chunk(handle, obj.polyline_points(), obj.polyline_points_count());
  }
}

void CurveRSCommandHandler::operator()(const CurveRSCommand::Internal::DeleteObject&) {
  const auto& handle = cmd_ctx.handle;
  renderer.m_.polylines.delete_chunk(handle);
  renderer.m_.curves.delete_chunk(handle);
  renderer.m_.helper_points.delete_chunk(handle);
  renderer.rs_.erase(handle);
}

void CurveRSCommandHandler::operator()(const CurveRSCommand::Internal::UpdateControlPoints&) {
  const auto& handle = cmd_ctx.handle;
  if (auto o = scene.arena<Curve>().get_obj(handle)) {
    auto& obj = *o.value();

    renderer.m_.curves.update_chunk(handle, obj.bezier3_points());
    if (renderer.rs_.at(handle).show_polyline) {
      renderer.m_.polylines.update_chunk(handle, obj.polyline_points(), obj.polyline_points_count());
    }
  } else {
    renderer.push_cmd(CurveRSCommand(handle, CurveRSCommand::Internal::DeleteObject{}));
  }
}

void CurveRSCommandHandler::operator()(const CurveRSCommand::UpdateObjectVisibility& cmd) {
  const auto& handle = cmd_ctx.handle;
  if (!renderer.rs_.contains(handle)) {
    util::Logger::warn(
        "OpenGL Renderer received update UpdateObjectVisibility command but there is no rendering state created");
    return;
  }

  auto& obj_rs = renderer.rs_.at(handle);
  if (obj_rs.visibility == cmd.new_visibility_state) {
    return;
  }
  renderer.m_.curves.delete_chunk(handle);
  renderer.m_.polylines.delete_chunk(handle);
  obj_rs.visibility = cmd.new_visibility_state;
}

void CurveRSCommandHandler::operator()(const CurveRSCommand::ShowPolyline& cmd) {
  const auto& handle = cmd_ctx.handle;
  if (!renderer.rs_.contains(handle)) {
    util::Logger::warn("OpenGL Renderer received update ShowPolyline command but there is no rendering state created");
    return;
  }

  auto& obj_rs         = renderer.rs_.at(handle);
  obj_rs.show_polyline = cmd.show;

  if (!cmd.show) {
    renderer.m_.polylines.delete_chunk(handle);
  } else {
    if (auto o = scene.arena<Curve>().get_obj(handle)) {
      auto& obj = *o.value();
      renderer.m_.polylines.update_chunk(handle, obj.polyline_points(), obj.polyline_points_count());
    }
  }
}

void CurveRSCommandHandler::operator()(const CurveRSCommand::ShowBernsteinControlPoints&) {
  // TODO(migoox): update bernstein points visibility
}

void CurveRSCommandHandler::operator()(const CurveRSCommand::UpdateHelperPoints&) {
  const auto& handle = cmd_ctx.handle;
  if (auto o = scene.arena<Curve>().get_obj(handle)) {
    auto& obj = *o.value();
    std::visit(eray::util::match{
                   [&](const BSplineCurve& curve) {
                     renderer.m_.helper_points.update_chunk(handle, curve.unique_bezier3_points(obj),
                                                            curve.unique_bezier3_points_count(obj));
                   },
                   [](const auto&) {},
               },
               obj.object);
    renderer.m_.curves.update_chunk(obj.handle(), obj.bezier3_points());
    renderer.m_.polylines.update_chunk(obj.handle(), obj.polyline_points(), obj.polyline_points_count());
  } else {
    renderer.push_cmd(CurveRSCommand(handle, CurveRSCommand::Internal::DeleteObject{}));
  }
}

CurvesRenderer CurvesRenderer::create() {
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

  return CurvesRenderer(Members{
      .helper_points_vao = std::move(helper_points_vao),
      .helper_points     = PointsChunksBuffer::create(),
      .polylines_vao     = std::move(polylines_vao),
      .polylines         = PointsChunksBuffer::create(),
      .curves_vao        = std::move(curves_vao),
      .curves            = PointsChunksBuffer::create(),
  });
}

CurvesRenderer::CurvesRenderer(Members&& members) : m_(std::move(members)) {}

void CurvesRenderer::update_impl(Scene& /*scene*/) {
  m_.polylines.sync(m_.polylines_vao.vbo().handle());
  m_.curves.sync(m_.curves_vao.vbo().handle());
  m_.helper_points.sync(m_.helper_points_vao.vbo().handle());
}

std::optional<std::pair<CurveHandle, size_t>> CurvesRenderer::find_helper_point_by_idx(size_t idx) const {
  return m_.helper_points.find_by_idx(idx);
}

void CurvesRenderer::render_polylines() const {
  m_.polylines_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_.polylines.count())));
}

void CurvesRenderer::render_curves() const {
  ERAY_GL_CALL(glPatchParameteri(GL_PATCH_VERTICES, 4));
  m_.curves_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_PATCHES, 0, static_cast<GLsizei>(m_.curves.count())));
}

void CurvesRenderer::render_helper_points() const {
  m_.helper_points_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_.helper_points.count())));
}

}  // namespace mini::gl
