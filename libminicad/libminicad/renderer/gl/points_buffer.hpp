#pragma once

#include <algorithm>
#include <generator>
#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/math/vec.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace mini::gl {

/**
 * @brief Synchronizes CPU and GPU buffers of points.
 *
 * @tparam Handle
 */
template <::mini::CObjectHandle Handle>
class PointsBuffer {
 public:
  PointsBuffer() = delete;

  static PointsBuffer create() {
    auto layout = eray::driver::gl::VertexBuffer::Layout();
    layout.add_attribute<float>("pos", 0, 3);

    return PointsBuffer<Handle>(eray::driver::gl::VertexArray::create(
        eray::driver::gl::VertexBuffer::create(std::move(layout)), eray::driver::gl::ElementBuffer::create()));
  }

  /**
   * @brief Synchronizes CPU and GPU buffers of points if the CPU buffer is dirty. Call it after
   * all of the required buffer modifications are applied.
   *
   */
  void sync() {
    vao_.vbo().buffer_data(std::span{points_}, eray::driver::gl::DataUsage::StaticDraw);
    is_dirty_ = false;
  }

  void update_points_owner(const Handle& handle, std::generator<eray::math::Vec3f> points, size_t count) {
    if (points_range_.contains(handle)) {
      update_points(handle, std::move(points), count);
      return;
    }

    auto begin_idx = points_.size();
    auto end_idx   = begin_idx;
    for (const auto& p : points) {
      points_.push_back(p.x);
      points_.push_back(p.y);
      points_.push_back(p.z);
      end_idx += 3;
    }

    points_range_.emplace(handle, PointsRange{.begin_idx = begin_idx, .end_idx = end_idx});
    is_dirty_ = true;
  }

  void delete_points_owners(std::span<Handle> handles) {
    namespace v = std::views;
    namespace r = std::ranges;

    auto point_its = handles                                                                  //
                     | v::transform([&](const auto& h) { return points_range_.find(h); })     //
                     | v::filter([&](const auto& it) { return it != points_range_.end(); });  //

    auto point_ranges = point_its                                                   //
                        | v::transform([&](const auto& it) { return it->second; })  //
                        | r::to<std::vector<PointsRange>>();                        //

    r::sort(point_ranges, [](const auto& pr1, const auto& pr2) { return pr1.begin_idx < pr2.begin_idx; });
    point_ranges.push_back({
        .begin_idx = points_.size(),
        .end_idx   = points_.size(),
    });

    auto point_ranges_pairs   = point_ranges | v::adjacent<2>;
    auto removed_points_count = 0U;
    for (const auto& [left, right] : point_ranges_pairs) {
      for (auto i = left.begin_idx, j = right.begin_idx; j < right.end_idx; ++i, ++j) {
        points_[i] = points_[j];
      }
      removed_points_count += left.size();
    }
    points_.resize(points_.size() - removed_points_count);

    for (auto it : point_its) {
      points_range_.erase(it);
    }
    is_dirty_ = true;
  }

  void bind() { vao_.bind(); }

  [[nodiscard]] size_t points_count() { return points_.size() / 3; }
  [[nodiscard]] size_t size() { return points_.size(); }

 private:
  void update_points(const Handle& handle, std::generator<eray::math::Vec3f> points, size_t count) {
    auto it = points_range_.find(handle);
    if (it == points_range_.end()) {
      return;
    }
    auto range = it->second;

    if (count == range.begin_idx) {
      for (auto i = range.begin_idx; const auto& p : points) {
        points_[i]     = p.x;
        points_[i + 1] = p.y;
        points_[i + 2] = p.z;
        i += 3;
      }

    } else {
      for (auto i = range.begin_idx, j = range.end_idx; j < points_.size(); ++i, ++j) {
        points_[i] = points_[j];
      }
      points_.resize(points_.size() - range.size());

      auto begin_idx = points_.size();
      auto end_idx   = begin_idx;
      for (const auto& p : points) {
        points_.push_back(p.x);
        points_.push_back(p.y);
        points_.push_back(p.z);
        end_idx += 3;
      }
      it->second = PointsRange{.begin_idx = begin_idx, .end_idx = end_idx};
    }

    is_dirty_ = true;
  }

  explicit PointsBuffer(eray::driver::gl::VertexArray vao) : is_dirty_(false), vao_(std::move(vao)) {}

 private:
  struct PointsRange {
    size_t begin_idx;
    size_t end_idx;  // non-inclusive

    size_t size() { return end_idx - begin_idx; }
  };

  bool is_dirty_;
  std::vector<float> points_;
  std::unordered_map<Handle, PointsRange> points_range_;
  eray::driver::gl::VertexArray vao_;
};

}  // namespace mini::gl
