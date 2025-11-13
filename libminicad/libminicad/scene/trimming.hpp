#pragma once

#include <libminicad/algorithm/intersection_finder.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/handles.hpp>

namespace mini {

struct ParamSpaceTrimmingData {
  static ParamSpaceTrimmingData from_intersection_curve(ISceneRenderer& renderer,
                                                        const IntersectionFinder::ParamSpace& param_space,
                                                        const ParametricSurfaceHandle& intersecting_surface_handle);

  static constexpr auto kCPUTrimmingTxtSize = IntersectionFinder::Curve::kTxtSize;
  static constexpr auto kGPUTrimmingTxtSize = 128;

  const TextureHandle& get_current_trimming_variant_txt() {
    return reverse ? trimming_variant_txt[1] : trimming_variant_txt[0];
  }
  const std::vector<uint32_t>& get_current_trimming_variant_txt_data() {
    return reverse ? trimming_variant_txt_data[1] : trimming_variant_txt_data[0];
  }

  TextureHandle curve_txt;
  std::array<TextureHandle, 2> trimming_variant_txt;
  std::array<std::vector<uint32_t>, 2> trimming_variant_txt_data;
  ParametricSurfaceHandle intersecting_surface_handle;
  bool reverse = false;
  bool enable  = false;
};

class ParamSpaceTrimmingDataManager {
 public:
  static ParamSpaceTrimmingDataManager create();

  static constexpr auto kCPUTrimmingTxtSize = ParamSpaceTrimmingData::kCPUTrimmingTxtSize;
  static constexpr auto kGPUTrimmingTxtSize = ParamSpaceTrimmingData::kGPUTrimmingTxtSize;

  void add(ParamSpaceTrimmingData&& data);
  void remove(uint32_t idx);

  std::vector<ParamSpaceTrimmingData>& data() { return data_; }
  const std::vector<ParamSpaceTrimmingData>& data() const { return data_; }

  void update_final_txt(bool force = false);

  const std::vector<uint32_t>& final_txt();
  const std::vector<ParametricSurfaceHandle>& final_intersecting_surfaces_handles() const;
  const std::vector<uint32_t>& final_txt_gpu();

  void mark_dirty() { dirty_ = true; }

 private:
  ParamSpaceTrimmingDataManager() = default;

  std::vector<ParametricSurfaceHandle> final_intersecting_surfaces_handles_;
  std::vector<ParamSpaceTrimmingData> data_;
  std::vector<uint32_t> final_txt_;
  std::vector<uint32_t> final_txt_gpu_;
  bool dirty_ = false;
};

}  // namespace mini
