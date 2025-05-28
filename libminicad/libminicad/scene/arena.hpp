#pragma once

#include <cstdint>
#include <expected>
#include <generator>
#include <liberay/util/logger.hpp>
#include <liberay/util/ruleof.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <optional>
#include <stack>
#include <vector>

#include "libminicad/scene/scene_object_handle.hpp"

namespace mini {

template <CObject Object>
class Arena {
 public:
  using Handle = eray::util::Handle<Object>;
  Arena()      = default;
  ERAY_DEFAULT_MOVE(Arena)
  ERAY_DELETE_COPY(Arena)

  enum class ObjectCreationError : uint8_t { ReachedMaxObjects = 0 };

  std::optional<Handle> handle_by_obj_id(std::uint32_t id) const {
    if (id >= max_objs_) {
      return std::nullopt;
    }

    if (!objects_[id]) {
      return std::nullopt;
    }

    return objects_[id]->first.handle();
  }

  [[nodiscard]] ObserverPtr<Object> unsafe_get_obj(const Handle& handle) {
    return ObserverPtr<Object>(objects_[handle.obj_id]->first);
  }

  [[nodiscard]] OptionalObserverPtr<Object> get_obj(const Handle& handle) {
    if (!exists(handle)) {
      return std::nullopt;
    }

    return OptionalObserverPtr<Object>(objects_[handle.obj_id]->first);
  }

  [[nodiscard]] OptionalObserverPtr<const Object> get_obj(const Handle& handle) const {
    if (!exists(handle)) {
      return std::nullopt;
    }

    return OptionalObserverPtr<const Object>(objects_[handle.obj_id]->first);
  }

  [[nodiscard]] bool exists(const Handle& handle) const {
    if (handle.owner_signature != signature_) {
      return false;
    }

    if (handle.obj_id > max_objs_) {
      return false;
    }

    if (!objects_[handle.obj_id]) {
      return false;
    }

    if (objects_[handle.obj_id]->first.handle().timestamp != handle.timestamp) {
      return false;
    }

    return true;
  }

  const std::vector<Handle>& objs_handles() const { return objects_order_; }

  auto objs() const {
    return objects_order_ | std::ranges::views::transform([this](const Handle& handle) -> const Object& {
             return objects_[handle.obj_id]->first;
           });
  }

  auto objs() {
    return objects_order_ | std::ranges::views::transform(
                                [this](const Handle& handle) -> Object& { return objects_[handle.obj_id]->first; });
  }

  std::uint32_t curr_obj_idx() const { return object_idx_; }

  void clear() {
    auto cpy = std::vector(objects_order_);
    delete_many(cpy);
  }

 protected:
  friend Scene;

  void init(std::size_t max_objs, std::uint32_t signature) {
    max_objs_  = max_objs;
    signature_ = signature;
    for (auto i = max_objs; i >= 1; --i) {
      objects_freed_.push(static_cast<uint32_t>(i));
    }
    objects_freed_.push(0U);
    objects_.resize(max_objs);
  }

  std::expected<ObserverPtr<Object>, ObjectCreationError> create_and_get(Scene& scene, Object::Variant&& variant) {
    if (objects_freed_.empty()) {
      eray::util::Logger::warn("Reached limit of objects. Available {}. Requested {}.", 0, 1);
      return std::unexpected(ObjectCreationError::ReachedMaxObjects);
    }
    auto h = unsafe_create(scene, std::move(variant));
    return ObserverPtr<Object>(objects_[h.obj_id]->first);
  }

  std::expected<Handle, ObjectCreationError> create(Scene& scene, Object::Variant&& variant) {
    if (objects_freed_.empty()) {
      eray::util::Logger::warn("Reached limit of objects. Available {}. Requested {}.", 0, 1);
      return std::unexpected(ObjectCreationError::ReachedMaxObjects);
    }

    return unsafe_create(scene, std::move(variant));
  }

  std::expected<std::vector<Handle>, ObjectCreationError> create_many(ref<Scene> scene, Object::Variant variant,
                                                                      size_t count) {
    if (objects_freed_.size() < count) {
      eray::util::Logger::warn("Reached limit of objects. Available {}. Requested {}.", objects_freed_.size(), count);
      return std::unexpected(ObjectCreationError::ReachedMaxObjects);
    }

    auto result = std::vector<Handle>();
    result.reserve(count);
    for (auto i = 0U; i < count; ++i) {
      result.emplace_back(unsafe_create(scene, typename Object::Variant(variant)));  // copy the variant
    }

    return result;
  }

  bool delete_obj(const Handle& handle) {
    if (!exists(handle)) {
      return false;
    }
    if (!objects_[handle.obj_id]->first.can_be_deleted()) {
      eray::util::Logger::warn("Requested deletion of an object, however it cannot be deleted.");
      return false;
    }
    objects_[handle.obj_id]->first.on_delete();

    auto ind = objects_[handle.obj_id]->second;
    for (size_t i = ind + 1; i < objects_order_.size(); ++i) {
      --objects_[objects_order_[i].obj_id]->second;
    }
    objects_order_.erase(objects_order_.begin() + static_cast<int>(ind));
    objects_[handle.obj_id] = std::nullopt;
    objects_freed_.push(handle.obj_id);

    return true;
  }

  bool delete_many(const std::vector<Handle>& handles) {
    size_t count = 0;
    for (const auto& h : handles) {
      if (!exists(h)) {
        continue;
      }
      ++count;

      auto idx                   = objects_[h.obj_id]->second;
      objects_order_[idx].obj_id = static_cast<uint32_t>(max_objs_);  // mark handle as invalid
      objects_[h.obj_id]         = std::nullopt;
      objects_freed_.push(h.obj_id);
    }

    for (auto i = 0U, j = 0U; i < objects_order_.size(); ++i) {
      if (objects_order_[i].obj_id != max_objs_) {  // if valid
        objects_order_[j++] = objects_order_[i];
      }
    }
    objects_order_.resize(objects_order_.size() - count, Handle(0U, 0U, 0U));

    for (auto i = 0U; auto& h : objects_order_) {
      objects_[h.obj_id]->second = i++;
    }

    return count > 0;
  }

  Object& unsafe_at(const Handle& handle) { return objects_.at(handle.obj_id)->first; }

 private:
  std::uint32_t timestamp() { return curr_timestamp_++; }

  Handle unsafe_create(ref<Scene> scene, Object::Variant&& variant) {
    auto obj_id = objects_freed_.top();
    objects_freed_.pop();
    auto h = Handle(signature_, timestamp(), obj_id);
    objects_order_.push_back(h);
    objects_[obj_id].emplace(std::piecewise_construct, std::forward_as_tuple(h, scene.get()),
                             std::forward_as_tuple(objects_order_.size() - 1));
    objects_[obj_id]->first.object = std::move(variant);
    objects_[obj_id]->first.set_name(std::format("{} {}", objects_[obj_id]->first.type_name(), object_idx_++));
    return h;
  }

 private:
  std::size_t max_objs_{0};
  std::uint32_t signature_{0};
  std::uint32_t curr_timestamp_{0};
  std::uint32_t object_idx_{0};

  std::vector<Handle> objects_order_;
  std::vector<std::optional<std::pair<Object, std::uint32_t>>> objects_;
  std::stack<std::uint32_t> objects_freed_;
};

}  // namespace mini
