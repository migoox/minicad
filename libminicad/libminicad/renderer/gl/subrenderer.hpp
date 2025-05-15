#pragma once
#include <optional>
#include <unordered_map>
#include <vector>

namespace mini::gl {

template <typename Handle, typename RenderingState, typename RenderingStateCommand>
class SubRenderer {
 public:
  void push_cmd(const RenderingStateCommand& cmd) { cmds_.emplace_back(cmd); }

  std::optional<RenderingState> object_rs(const Handle& handle) {
    auto it = rs_.find(handle);
    if (it == rs_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  void set_object_rs(const Handle& handle, const RenderingState& state) { rs_[handle] = state; }

 protected:
  std::vector<RenderingStateCommand> cmds_;
  std::unordered_map<Handle, RenderingState> rs_;
};

}  // namespace mini::gl
