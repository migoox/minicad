#pragma once

#include <libminicad/algorithm/intersection_finder.hpp>
#include <libminicad/renderer/scene_renderer.hpp>

namespace mini {

struct ParamSpaceTrimmingData {
  static ParamSpaceTrimmingData from_intersection_curve(ISceneRenderer& renderer,
                                                        const IntersectionFinder::ParamSpace& param_space);

  const TextureHandle& get_current_trimming_variant_txt() {
    return reverse ? trimming_variant_txt[1] : trimming_variant_txt[0];
  }
  const std::vector<uint32_t>& get_current_trimming_variant_txt_data() {
    return reverse ? trimming_variant_txt_data[1] : trimming_variant_txt_data[0];
  }

  TextureHandle curve_txt;
  std::array<TextureHandle, 2> trimming_variant_txt;
  std::array<std::vector<uint32_t>, 2> trimming_variant_txt_data;
  bool reverse = false;
  bool enable  = false;
};

class ParamSpaceTrimmingDataManager {
 public:
  static ParamSpaceTrimmingDataManager create(size_t width, size_t height);

  void add(ParamSpaceTrimmingData&& data);
  void remove(uint32_t idx);

  std::vector<ParamSpaceTrimmingData>& data() { return data_; }
  const std::vector<ParamSpaceTrimmingData>& data() const { return data_; }

  void update_final_txt(bool force = false);

  const std::vector<uint32_t>& final_txt();

  void mark_dirty() { dirty_ = true; }

  size_t width() const { return width_; }
  size_t height() const { return height_; }

 private:
  ParamSpaceTrimmingDataManager(size_t width, size_t height) : width_(width), height_(height) {}

  std::vector<ParamSpaceTrimmingData> data_;
  std::vector<uint32_t> final_txt_;
  bool dirty_ = false;
  size_t width_;
  size_t height_;
};

}  // namespace mini
