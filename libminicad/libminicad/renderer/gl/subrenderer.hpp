#pragma once
#include <algorithm>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/scene.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mini::gl {

template <typename SubRendererImpl, typename Handle, typename RenderingState, typename RenderingStateCommand,
          typename RenderingStateCommandHandler>
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

  void update(Scene& scene) {
    std::ranges::sort(cmds_, [](const auto& x, const auto& y) {
      return std::visit(kRSCommandPriorityComparer, x.variant, y.variant);
    });

    for (const auto& cmd : cmds_) {
      auto handler = RenderingStateCommandHandler(cmd, *static_cast<SubRendererImpl*>(this), scene);
      std::visit(handler, cmd.variant);
    }
    cmds_.clear();

    static_cast<SubRendererImpl*>(this)->update_impl(scene);
  }

 protected:
  std::vector<RenderingStateCommand> cmds_;
  std::unordered_map<Handle, RenderingState> rs_;
};

}  // namespace mini::gl
