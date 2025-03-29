#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini {

void PointListObject::mark_dirty() { scene_.mark_dirty(handle_); }

void SceneObject::mark_dirty() { scene_.mark_dirty(handle_); }

}  // namespace mini
