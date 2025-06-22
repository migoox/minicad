#include <libminicad/renderer/gl/intersection_curves_renderer.hpp>
#include <libminicad/scene/intersection_curve.hpp>

namespace mini::gl {

std::generator<eray::math::Vec3f> IntersectionCurveRSCommandHandler::lines_generator(ref<IntersectionCurve> curve) {
  auto points = curve.get().points();
  for (auto i = 0U; i < points.size() - 1; ++i) {
    co_yield points[i];
    co_yield points[i + 1];
  }
}

void IntersectionCurveRSCommandHandler::operator()(const IntersectionCurveRSCommand::Internal::AddObject&) {
  const auto& handle = cmd_ctx.handle;

  if (auto opt = scene.arena<IntersectionCurve>().get_obj(handle)) {
    auto& obj = **opt;
    renderer.m_.curves.update_chunk(handle, lines_generator(obj), obj.points().size() * 2);
  }
}

void IntersectionCurveRSCommandHandler::operator()(const IntersectionCurveRSCommand::Internal::DeleteObject&) {
  const auto& handle = cmd_ctx.handle;
  renderer.m_.curves.delete_chunk(handle);
  renderer.rs_.erase(handle);
}

IntersectionCurvesRenderer IntersectionCurvesRenderer::create() {
  auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  auto curves_vao =
      eray::driver::gl::SimpleVertexArray::create(eray::driver::gl::VertexBuffer::create(std::move(vbo_layout)));

  return IntersectionCurvesRenderer(Members{
      .curves_vao = std::move(curves_vao),        //
      .curves     = PointsChunksBuffer::create()  //
  });
}

IntersectionCurvesRenderer::IntersectionCurvesRenderer(Members&& members) : m_(std::move(members)) {}

void IntersectionCurvesRenderer::update_impl(Scene& /*scene*/) { m_.curves.sync(m_.curves_vao.vbo().handle()); }

void IntersectionCurvesRenderer::render_curves() const {
  m_.curves_vao.bind();
  ERAY_GL_CALL(glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_.curves.count())));
}

}  // namespace mini::gl
