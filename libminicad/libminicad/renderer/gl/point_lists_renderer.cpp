#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <libminicad/renderer/gl/point_lists_renderer.hpp>
#include <libminicad/renderer/gl/points_buffer.hpp>
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
  renderer.curves_.add_points_owner(handle, obj.bezier3_points(), obj.bezier3_points_count());
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::Internal::UpdateControlPoints&) {
  const auto& handle = cmd_ctx.handle;
  renderer.curves_.update_points(handle, obj.bezier3_points(), obj.bezier3_points_count());
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::UpdateObjectVisibility& cmd) {
  if (obj_rs.visibility == cmd.new_visibility_state) {
    return;
  }
  const auto& handle = cmd_ctx.handle;
  renderer.curves_to_delete_.push_back(handle);
  obj_rs.visibility = cmd.new_visibility_state;
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::ShowPolyline& cmd) {
  if (obj_rs.show_polyline == cmd.show) {
    return;
  }
  const auto& handle = cmd_ctx.handle;  // NOLINT
  // TODO(migoox): polylines
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::ShowBernsteinControlPoints&) {
  // TODO(migoox): update bernstein points visibility
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::UpdateBernsteinControlPoints&) {
  // TODO(migoox)
}

PointListsRenderer PointListsRenderer::create() {
  return PointListsRenderer(PointsBuffer<PointListObjectHandle>::create(),
                            PointsBuffer<PointListObjectHandle>::create(),
                            PointsBuffer<PointListObjectHandle>::create());
}
PointListsRenderer::PointListsRenderer(PointsBuffer<PointListObjectHandle>&& helper_points,
                                       PointsBuffer<PointListObjectHandle>&& polylines,
                                       PointsBuffer<PointListObjectHandle>&& curves)
    : helper_points_(std::move(helper_points)), polylines_(std::move(polylines)), curves_(std::move(curves)) {}

void PointListsRenderer::push_cmd(const PointListObjectRSCommand& cmd) { cmds_.push_back(cmd); }

void PointListsRenderer::update(Scene& scene) {
  // TODO(migoox): add commands preprocessing to remove repetitions

  // Flush the commands and invoke handler
  for (auto& cmd : cmds_) {
    auto add_obj    = std::holds_alternative<PointListObjectRSCommand::Internal::AddObject>(cmd.variant);
    auto delete_obj = std::holds_alternative<PointListObjectRSCommand::Internal::DeleteObject>(cmd.variant);

    if (add_obj) {
      point_lists_.emplace(cmd.handle, mini::PointListObjectRS(VisibilityState::Visible));
    }
    if (delete_obj) {
      curves_to_delete_.push_back(cmd.handle);
      point_lists_.erase(cmd.handle);
    }
    if (!point_lists_.contains(cmd.handle)) {
      continue;
    }

    if (auto obj = scene.get_obj(cmd.handle)) {
      auto handler = PointListObjectRSCommandHandler(cmd, *this, *obj.value(), point_lists_.at(cmd.handle));
      std::visit(handler, cmd.variant);
    } else {
      point_lists_.erase(cmd.handle);
    }
  }
  cmds_.clear();

  // Remove point owners
  if (!curves_to_delete_.empty()) {
    curves_.delete_points_owners(std::span{curves_to_delete_});
    curves_to_delete_.clear();
  }

  // Sync the buffer
  curves_.sync();
}

void PointListsRenderer::render_polylines() {
  polylines_.bind();
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(polylines_.points_count()));
}

void PointListsRenderer::render_curves() {
  glPatchParameteri(GL_PATCH_VERTICES, 4);
  curves_.bind();
  auto p = curves_.points_count();
  glDrawArrays(GL_PATCHES, 0, static_cast<GLsizei>(p));
}

void PointListsRenderer::render_helper_points() {
  // TODO(migoox)
}

std::optional<PointListObjectRS> PointListsRenderer::object_rs(const PointListObjectHandle& handle) {
  if (!point_lists_.contains(handle)) {
    return std::nullopt;
  }

  return point_lists_.at(handle);
}

void PointListsRenderer::set_object_rs(const PointListObjectHandle& handle, const ::mini::PointListObjectRS& state) {
  if (point_lists_.contains(handle)) {
    point_lists_.at(handle) = state;
  }
}

}  // namespace mini::gl
