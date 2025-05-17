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

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::AddObject&) {}
void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::DeleteObject&) {}

void PatchSurfaceRSCommandHandler::operator()(const PatchSurfaceRSCommand::Internal::UpdateControlPoints&) {}

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
      .control_grids_vao = std::move(control_grids_vao),
      .control_grids     = PointsChunksBuffer::create(),
  });
}

PatchSurfaceRenderer::PatchSurfaceRenderer(Members&& members) : m_(std::move(members)) {}

void PatchSurfaceRenderer::update_impl(Scene& /*scene*/) {
  m_.surfaces.sync(m_.surfaces_vao.vbo().handle());
  m_.control_grids.sync(m_.control_grids_vao.vbo().handle());
}

void PatchSurfaceRenderer::render_control_grids() const {}

void PatchSurfaceRenderer::render_surfaces() const {}

}  // namespace mini::gl
