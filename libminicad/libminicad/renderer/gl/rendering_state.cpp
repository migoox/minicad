#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <variant>

namespace mini::gl {

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
