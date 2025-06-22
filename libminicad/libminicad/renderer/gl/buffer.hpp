#pragma once

#include <algorithm>
#include <generator>
#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/util/generator.hpp>
#include <liberay/util/ruleof.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mini::gl {

struct Chunk {
  size_t begin_idx;  // inclusive
  size_t end_idx;    // non-inclusive

  size_t size() const { return end_idx - begin_idx; }
};

/**
 * @brief Represents a generic contiguous buffer of GPU primitives organized into chunks stored on CPU. Each
 * chunk corresponds to rendered entity. This class is particularily useful for batched rendering. To synchronize
 * the buffer with GPU, use sync() method, note that only DSA OpenGL buffers are supported in this method.
 *
 * @tparam ChunkOwnerHandle
 * @tparam CPUSourceType
 * @tparam GPUTargetPrimitiveType
 * @tparam GPUTargetPrimitiveCount
 * @tparam (*TypeInserter)(const CPUSourceType&, GPUTargetPrimitiveType*)
 */
template <::mini::CObjectHandle ChunkOwnerHandle, typename CPUSourceType, typename GPUTargetPrimitiveType,
          std::size_t GPUTargetPrimitiveCount, void (*TypeInserter)(const CPUSourceType&, GPUTargetPrimitiveType*)>
class ChunksBuffer {
 public:
  ERAY_DEFAULT_MOVE(ChunksBuffer)
  ERAY_DELETE_COPY(ChunksBuffer)

  static ChunksBuffer create() {
    return ChunksBuffer<ChunkOwnerHandle, CPUSourceType, GPUTargetPrimitiveType, GPUTargetPrimitiveCount,
                        TypeInserter>();
  }

  static constexpr std::size_t kGPUTargetPrimitiveCount = GPUTargetPrimitiveCount;

  /**
   * @brief Synchronizes CPU and GPU buffers when the CPU buffer is dirty. Call it after
   * all of the required buffer modifications are applied. DSA buffer is expected.
   *
   */
  void sync(const eray::driver::gl::BufferHandle& dsa_buffer_handle) {
    if (!expired_chunks_.empty()) {
      delete_expired_chunks();
    }
    if (is_dirty_) {
      ERAY_GL_CALL(glNamedBufferData(dsa_buffer_handle.get(),
                                     static_cast<GLsizeiptr>(data_.size() * sizeof(GPUTargetPrimitiveType)),
                                     reinterpret_cast<const void*>(data_.data()), GL_STATIC_DRAW));
    }
    is_dirty_ = false;
  }

  void update_chunk(const ChunkOwnerHandle& owner, const std::vector<CPUSourceType>& data) {
    update_chunk(owner, std::move(eray::util::container_to_generator(std::ref(data))), data.size());
  }

  void update_chunk(const ChunkOwnerHandle& owner, std::generator<CPUSourceType> data, size_t count) {
    if (chunk_range_.contains(owner)) {
      update_chunk_values(owner, std::move(data), count);
      return;
    }

    auto begin_idx = data_.size();
    auto end_idx   = begin_idx;
    for (const auto& p : data) {
      data_.resize(data_.size() + kGPUTargetPrimitiveCount);
      TypeInserter(p, &data_[data_.size() - kGPUTargetPrimitiveCount]);
      end_idx += kGPUTargetPrimitiveCount;
    }

    chunk_range_.emplace(owner, Chunk{.begin_idx = begin_idx, .end_idx = end_idx});
    is_dirty_ = true;
  }

  void delete_chunk(const ChunkOwnerHandle& owner) { expired_chunks_.insert(owner); }

  std::optional<std::pair<ChunkOwnerHandle, size_t>> find_by_idx(size_t idx) const {
    auto primitive_idx = idx * kGPUTargetPrimitiveCount;
    // TODO(migoox): optimize it
    for (const auto& [handle, chunk] : chunk_range_) {
      if (primitive_idx >= chunk.begin_idx && primitive_idx < chunk.end_idx) {
        auto offset = (primitive_idx - chunk.begin_idx) / kGPUTargetPrimitiveCount;
        return std::pair(handle, offset);
      }
    }

    return std::nullopt;
  }

  [[nodiscard]] size_t count() const { return data_.size() / kGPUTargetPrimitiveCount; }
  [[nodiscard]] size_t size() const { return data_.size(); }

 private:
  ChunksBuffer() = default;

  void update_chunk_values(const ChunkOwnerHandle& owner, std::generator<CPUSourceType> data, size_t count) {
    auto it = chunk_range_.find(owner);
    if (it == chunk_range_.end()) {
      return;
    }
    auto range = it->second;

    if (kGPUTargetPrimitiveCount * count == range.size()) {
      // update values only
      for (auto i = range.begin_idx; const auto& p : data) {
        TypeInserter(p, &data_[i]);
        i += kGPUTargetPrimitiveCount;
      }

    } else {
      // the size has changed
      for (auto i = range.begin_idx, j = range.end_idx; j < data_.size(); ++i, ++j) {
        data_[i] = data_[j];
      }
      data_.resize(data_.size() - range.size());

      for (auto& r : chunk_range_ | std::views::values) {
        if (r.begin_idx > range.begin_idx) {
          r.begin_idx -= range.size();
          r.end_idx -= range.size();
        }
      }

      auto begin_idx = data_.size();
      auto end_idx   = begin_idx;
      for (const auto& p : data) {
        data_.resize(data_.size() + kGPUTargetPrimitiveCount);
        TypeInserter(p, &data_[data_.size() - kGPUTargetPrimitiveCount]);
        end_idx += kGPUTargetPrimitiveCount;
      }
      it->second = Chunk{.begin_idx = begin_idx, .end_idx = end_idx};
    }

    is_dirty_ = true;
  }

  void delete_expired_chunks() {
    namespace v = std::views;
    namespace r = std::ranges;

    if (expired_chunks_.empty()) {
      return;
    }

    auto point_its = expired_chunks_                                                         //
                     | v::transform([&](const auto& h) { return chunk_range_.find(h); })     //
                     | v::filter([&](const auto& it) { return it != chunk_range_.end(); });  //

    auto point_ranges = point_its                                                   //
                        | v::transform([&](const auto& it) { return it->second; })  //
                        | r::to<std::vector<Chunk>>();                              //

    r::sort(point_ranges, [](const auto& pr1, const auto& pr2) { return pr1.begin_idx < pr2.begin_idx; });
    point_ranges.push_back({
        .begin_idx = data_.size(),
        .end_idx   = data_.size(),
    });

    auto point_ranges_pairs   = point_ranges | v::adjacent<2>;
    auto removed_points_count = 0U;
    for (const auto& [left, right] : point_ranges_pairs) {
      for (auto i = left.begin_idx, j = right.begin_idx; j < right.end_idx; ++i, ++j) {
        data_[i] = data_[j];
      }
      removed_points_count += left.size();
    }
    data_.resize(data_.size() - removed_points_count);

    for (auto it : point_its) {
      chunk_range_.erase(it);
    }
    expired_chunks_.clear();
  }

 private:
  std::vector<GPUTargetPrimitiveType> data_;
  std::unordered_set<ChunkOwnerHandle> expired_chunks_;
  std::unordered_map<ChunkOwnerHandle, Chunk> chunk_range_;
  bool is_dirty_{};
};

}  // namespace mini::gl
