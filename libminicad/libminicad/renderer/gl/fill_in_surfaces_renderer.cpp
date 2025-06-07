#include <glad/gl.h>

#include <libminicad/renderer/gl/fill_in_surfaces_renderer.hpp>
#include <libminicad/scene/fill_in_suface.hpp>

namespace mini::gl {

std::generator<eray::math::Vec3f> FillInSurfaceRSCommandHandler::rational_bezier_patch_generator(
    ref<FillInSurface> surface) {
  const auto& rbp = surface.get().rational_bezier_points();
  for (auto i = 0U; i < FillInSurface::kNeighbors; ++i) {
    for (auto j = 0U; j < FillInSurface::kPatchPointsCount; ++j) {
      co_yield rbp[FillInSurface::kPatchPointsCount * i + j];
    }
    co_yield eray::math::Vec3f(static_cast<float>(surface.get().tess_level()), 0.F, 0.F);
  }
}

size_t FillInSurfaceRSCommandHandler::rational_bezier_patch_count(ref<FillInSurface> surface) {
  return surface.get().rational_bezier_points().size() + FillInSurface::kNeighbors;
}

void FillInSurfaceRSCommandHandler::operator()(const FillInSurfaceRSCommand::Internal::AddObject&) {
  const auto& handle = cmd_ctx.handle;

  renderer.rs_.emplace(handle, mini::FillInSurfaceRS(VisibilityState::Visible));

  if (auto o = scene.arena<FillInSurface>().get_obj(handle)) {
    auto& obj = *o.value();
    renderer.m_.surfaces.update_chunk(handle, rational_bezier_patch_generator(obj), rational_bezier_patch_count(obj));
    renderer.m_.control_grids.update_chunk(handle, obj.tangent_grid_points(), obj.tangent_grid_points_count());
  }
}

void FillInSurfaceRSCommandHandler::operator()(const FillInSurfaceRSCommand::Internal::DeleteObject&) {
  const auto& handle = cmd_ctx.handle;
  renderer.m_.surfaces.delete_chunk(handle);
  renderer.m_.control_grids.delete_chunk(handle);
  renderer.rs_.erase(handle);
}

void FillInSurfaceRSCommandHandler::operator()(const FillInSurfaceRSCommand::Internal::UpdateControlPoints&) {
  const auto& handle = cmd_ctx.handle;
  if (auto o = scene.arena<FillInSurface>().get_obj(handle)) {
    auto& obj = *o.value();
    renderer.m_.surfaces.update_chunk(handle, rational_bezier_patch_generator(obj), rational_bezier_patch_count(obj));
    renderer.m_.control_grids.update_chunk(handle, obj.tangent_grid_points(), obj.tangent_grid_points_count());
  } else {
    renderer.push_cmd(FillInSurfaceRSCommand(handle, FillInSurfaceRSCommand::Internal::DeleteObject{}));
  }
}

void FillInSurfaceRSCommandHandler::operator()(const FillInSurfaceRSCommand::UpdateObjectVisibility&) {}

void FillInSurfaceRSCommandHandler::operator()(const FillInSurfaceRSCommand::ShowPolyline&) {}

FillInSurfaceRenderer FillInSurfaceRenderer::create() {
  auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  auto control_grids_vao = eray::driver::gl::VertexArray::create(
      eray::driver::gl::VertexBuffer::create(eray::driver::gl::VertexBuffer::Layout(vbo_layout)),
      eray::driver::gl::ElementBuffer::create());
  auto surfaces_vao = eray::driver::gl::VertexArray::create(
      eray::driver::gl::VertexBuffer::create(eray::driver::gl::VertexBuffer::Layout(vbo_layout)),
      eray::driver::gl::ElementBuffer::create());

  return FillInSurfaceRenderer(Members{
      .surfaces_vao      = std::move(surfaces_vao),
      .surfaces          = PointsChunksBuffer::create(),
      .control_grids_vao = std::move(control_grids_vao),
      .control_grids     = PointsChunksBuffer::create(),
  });
}

FillInSurfaceRenderer::FillInSurfaceRenderer(Members&& members) : m_(std::move(members)) {}

void FillInSurfaceRenderer::update_impl(Scene& /*scene*/) {
  m_.surfaces.sync(m_.surfaces_vao.vbo().handle());
  m_.control_grids.sync(m_.control_grids_vao.vbo().handle());
}

void FillInSurfaceRenderer::render_control_grids() const {
  m_.control_grids_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_.control_grids.chunks_count())));
}

void FillInSurfaceRenderer::render_fill_in_surfaces() const {
  ERAY_GL_CALL(glPatchParameteri(GL_PATCH_VERTICES, 21));
  m_.surfaces_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_PATCHES, 0, static_cast<GLsizei>(m_.surfaces.chunks_count())));
}

}  // namespace mini::gl
