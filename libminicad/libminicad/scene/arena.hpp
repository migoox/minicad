#pragma once

#include <cstdint>
#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/ruleof.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <stack>
#include <vector>

namespace mini {

template <CObject Object>
class Arena {
 public:
  using Handle = eray::util::Handle<Object>;
  Arena()      = default;
  ERAY_DEFAULT_MOVE(Arena)
  ERAY_DISABLE_COPY(Arena)

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

  [[nodiscard]] OptionalObserverPtr<Object> get_obj(const Handle& handle) {
    if (!exists(handle)) {
      return std::nullopt;
    }

    return OptionalObserverPtr<Object>(objects_[handle.obj_id]->first);
  }

  [[nodiscard]] bool exists(const Handle& handle) {
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

  const auto& objs() const {
    return objects_order_ |
           std::ranges::views::transform([this](const Handle& handle) { return objects_[handle.obj_id].first; });
  }

  auto& objs() {
    return objects_order_ |
           std::ranges::views::transform([this](const Handle& handle) { return objects_[handle.obj_id].first; });
  }

  std::uint32_t curr_obj_idx() const { return object_idx_; }

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

  std::expected<Handle, ObjectCreationError> create(Scene& scene) {
    if (objects_freed_.empty()) {
      eray::util::Logger::warn("Reached limit of objects");
      return std::unexpected(ObjectCreationError::ReachedMaxObjects);
    }

    auto obj_id = objects_freed_.top();
    objects_freed_.pop();
    auto h = Handle(signature_, timestamp(), obj_id);
    objects_order_.push_back(h);
    objects_[obj_id].emplace(std::piecewise_construct, std::forward_as_tuple(h, scene),
                             std::forward_as_tuple(objects_order_.size() - 1));
    ++object_idx_;
    return h;
  }

  bool delete_obj(const Handle& handle) {
    if (!exists(handle)) {
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

  Object& at_unsafe(const Handle& handle) { return objects_.at(handle.obj_id)->first; }

 private:
  std::uint32_t timestamp() { return curr_timestamp_++; }

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
