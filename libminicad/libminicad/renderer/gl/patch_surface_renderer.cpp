#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/buffer.hpp>
#include <libminicad/renderer/gl/patch_surface_renderer.hpp>
#include <libminicad/renderer/gl/texture_array.hpp>
#include <libminicad/renderer/gl/trimming_texture_manager.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>

namespace mini::gl {

std::generator<eray::math::Vec3f> PatchSurfaceRSCommandHandler::bezier_patch_generator(ref<PatchSurface> surface) {
  const auto& rbp = surface.get().bezier3_points();

  auto id = static_cast<float>(renderer.m_.textures_manager.get_id(surface.get()));
  for (auto i = 0U; i < rbp.size();) {
    for (auto j = 0U; j < PatchSurface::kPatchSize * PatchSurface::kPatchSize; ++j) {
      co_yield rbp[i++];
    }
    co_yield eray::math::Vec3f(static_cast<float>(surface.get().tess_level()), id, 0.F);
  }
}

size_t PatchSurfaceRSCommandHandler::bezier_patch_count(ref<PatchSurface> surface) {
  return surface.get().bezier3_points().size() + FillInSurface::kNeighbors;
}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::UpdateTrimmingTextures&) {
  const auto& handle = cmd_ctx.handle;

  renderer.rs_.emplace(handle, mini::PatchSurfaceRS(VisibilityState::Visible));

  if (auto o = scene.arena<PatchSurface>().get_obj(handle)) {
    auto& obj = *o.value();

    renderer.m_.textures_manager.update(obj);
    renderer.m_.surfaces.update_chunk(handle, bezier_patch_generator(obj), bezier_patch_count(obj));
    renderer.m_.control_grids.update_chunk(handle, obj.control_grid_points(), obj.control_grid_points_count());
  }
}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::AddObject&) {
  const auto& handle = cmd_ctx.handle;

  renderer.rs_.emplace(handle, mini::PatchSurfaceRS(VisibilityState::Visible));

  if (auto o = scene.arena<PatchSurface>().get_obj(handle)) {
    auto& obj = *o.value();

    renderer.m_.textures_manager.update(obj);
    renderer.m_.surfaces.update_chunk(handle, bezier_patch_generator(obj), bezier_patch_count(obj));
    renderer.m_.control_grids.update_chunk(handle, obj.control_grid_points(), obj.control_grid_points_count());
  }
}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::DeleteObject&) {
  const auto& handle = cmd_ctx.handle;

  renderer.m_.textures_manager.remove(handle);
  renderer.m_.surfaces.delete_chunk(handle);
  renderer.m_.control_grids.delete_chunk(handle);
  renderer.rs_.erase(handle);
}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::UpdateControlPoints&) {
  const auto& handle = cmd_ctx.handle;
  if (auto o = scene.arena<PatchSurface>().get_obj(handle)) {
    auto& obj = *o.value();
    renderer.m_.surfaces.update_chunk(handle, bezier_patch_generator(obj), bezier_patch_count(obj));
    renderer.m_.control_grids.update_chunk(handle, obj.control_grid_points(), obj.control_grid_points_count());
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

  return PatchSurfaceRenderer(Members{
      .surfaces_vao      = std::move(surfaces_vao),
      .surfaces          = PointsChunksBuffer::create(),
      .textures_manager  = TrimmingTexturesManager<PatchSurface>::create(),
      .control_grids_vao = std::move(control_grids_vao),
      .control_grids     = PointsChunksBuffer::create(),
  });
}

PatchSurfaceRenderer::PatchSurfaceRenderer(Members&& members) : m_(std::move(members)) {}

void PatchSurfaceRenderer::update_impl(Scene& /*scene*/) {
  m_.surfaces.sync(m_.surfaces_vao.vbo().handle());
  m_.control_grids.sync(m_.control_grids_vao.vbo().handle());
}

void PatchSurfaceRenderer::render_control_grids() const {
  m_.control_grids_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_.control_grids.count())));
}

void PatchSurfaceRenderer::render_surfaces() const {
  ERAY_GL_CALL(glPatchParameteri(GL_PATCH_VERTICES, 17));
  ERAY_GL_CALL(glActiveTexture(GL_TEXTURE0));
  m_.surfaces_vao.bind();
  m_.textures_manager.txt_array().bind();
  ERAY_GL_CALL(glDrawArrays(GL_PATCHES, 0, static_cast<GLsizei>(m_.surfaces.count())));
}

}  // namespace mini::gl
