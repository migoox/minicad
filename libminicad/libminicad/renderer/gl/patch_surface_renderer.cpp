#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/buffer.hpp>
#include <libminicad/renderer/gl/patch_surface_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>

namespace mini::gl {

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::AddObject&) {
  const auto& handle = cmd_ctx.handle;

  renderer.rs_.emplace(handle, mini::PatchSurfaceRS(VisibilityState::Visible));

  if (auto o = scene.arena<PatchSurface>().get_obj(handle)) {
    auto& obj = *o.value();
    renderer.m_.surfaces.update_chunk(handle, obj.bezier3_points(), obj.bezier3_points_count());
    renderer.m_.surfaces_indices.update_chunk(handle, obj.bezier3_indices(), obj.bezier3_indices_count());
    renderer.m_.control_grids.update_chunk(handle, obj.control_points(), obj.control_points_count());
    renderer.m_.control_grids_indices.update_chunk(handle, obj.grids_indices(), obj.grids_indices_count());
  }
}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::DeleteObject&) {
  const auto& handle = cmd_ctx.handle;
  renderer.m_.surfaces.delete_chunk(handle);
  renderer.m_.surfaces_indices.delete_chunk(handle);
  renderer.m_.control_grids.delete_chunk(handle);
  renderer.m_.control_grids_indices.delete_chunk(handle);
  renderer.rs_.erase(handle);
}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::UpdateControlPoints&) {
  const auto& handle = cmd_ctx.handle;
  if (auto o = scene.arena<PatchSurface>().get_obj(handle)) {
    auto& obj = *o.value();
    renderer.m_.surfaces.update_chunk(handle, obj.bezier3_points(), obj.bezier3_points_count());
    renderer.m_.surfaces_indices.update_chunk(handle, obj.bezier3_indices(), obj.bezier3_indices_count());
    renderer.m_.control_grids.update_chunk(handle, obj.control_points(), obj.control_points_count());
    renderer.m_.control_grids_indices.update_chunk(handle, obj.grids_indices(), obj.grids_indices_count());
  } else {
    renderer.push_cmd(PatchSurfaceRSCommand(handle, PatchSurfaceRSCommand::Internal::DeleteObject{}));
  }
}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::UpdateObjectVisibility&) {}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::ShowPolyline&) {}

PatchSurfaceRenderer PatchSurfaceRenderer::create() {
  auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  auto control_grids_vao = eray::driver::gl::VertexArray::create(
      eray::driver::gl::VertexBuffer::create(eray::driver::gl::VertexBuffer::Layout(vbo_layout)),
      eray::driver::gl::ElementBuffer::create());
  auto surfaces_vao = eray::driver::gl::VertexArray::create(
      eray::driver::gl::VertexBuffer::create(eray::driver::gl::VertexBuffer::Layout(vbo_layout)),
      eray::driver::gl::ElementBuffer::create());

  return PatchSurfaceRenderer(Members{.surfaces_vao          = std::move(surfaces_vao),
                                      .surfaces              = PointsChunksBuffer::create(),
                                      .surfaces_indices      = IndicesChunksBuffer::create(),
                                      .control_grids_vao     = std::move(control_grids_vao),
                                      .control_grids         = PointsChunksBuffer::create(),
                                      .control_grids_indices = IndicesChunksBuffer::create()});
}

PatchSurfaceRenderer::PatchSurfaceRenderer(Members&& members) : m_(std::move(members)) {}

void PatchSurfaceRenderer::update_impl(Scene& /*scene*/) {
  m_.surfaces.sync(m_.surfaces_vao.vbo().handle());
  m_.surfaces_indices.sync(m_.surfaces_vao.ebo().handle());
  m_.control_grids.sync(m_.control_grids_vao.vbo().handle());
  m_.control_grids_indices.sync(m_.control_grids_vao.ebo().handle());
}

void PatchSurfaceRenderer::render_control_grids() const {
  m_.control_grids_vao.bind();
  ERAY_GL_CALL(glDrawElements(GL_LINES, m_.control_grids_indices.chunks_count(), GL_UNSIGNED_INT, nullptr));
}

void PatchSurfaceRenderer::render_surfaces() const {
  ERAY_GL_CALL(glPatchParameteri(GL_PATCH_VERTICES, 17));
  m_.surfaces_vao.bind();
  ERAY_GL_CALL(glDrawElements(GL_PATCHES, m_.surfaces_indices.chunks_count(), GL_UNSIGNED_INT, nullptr));
}

}  // namespace mini::gl
