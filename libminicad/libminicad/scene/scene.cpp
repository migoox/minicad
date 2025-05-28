#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace mini {

std::uint32_t Scene::next_signature_ = 0;

Scene::Scene(std::unique_ptr<ISceneRenderer>&& renderer)
    : renderer_(std::move(renderer)), signature_(next_signature_++) {
  arena<SceneObject>().init(kMaxObjects, signature_);
  arena<Curve>().init(kMaxObjects, signature_);
  arena<PatchSurface>().init(kMaxObjects, signature_);
}

bool Scene::push_back_point_to_curve(const SceneObjectHandle& p_handle, const CurveHandle& c_handle) {
  if (auto c = arena<Curve>().get_obj(c_handle)) {
    if (auto p = arena<SceneObject>().get_obj(p_handle)) {
      if (p.value()->has_type<Point>()) {
        return c.value()->push_back(p_handle).has_value();
      }
    }
  }
  return false;
}

bool Scene::remove_point_from_curve(const SceneObjectHandle& p_handle, const CurveHandle& c_handle) {
  if (auto c = arena<Curve>().get_obj(c_handle)) {
    if (auto p = arena<SceneObject>().get_obj(p_handle)) {
      if (p.value()->has_type<Point>()) {
        return c.value()->remove(p_handle).has_value();
      }
    }
  }

  return false;
}

void Scene::remove_from_order(size_t ind) {
  for (size_t i = ind + 1; i < objects_order_.size(); ++i) {
    std::visit(eray::util::match{[&](const auto& handle) {
                 using T = std::decay_t<decltype(handle)>::Object;
                 arena<T>().unsafe_at(handle).order_idx_--;
               }},
               objects_order_[i]);
  }
  objects_order_.erase(objects_order_.begin() + static_cast<int>(ind));
}

void Scene::clear() {
  std::apply([](auto&... arena) { ((arena.clear()), ...); }, arenas_);
  objects_order_.clear();
}

}  // namespace mini
