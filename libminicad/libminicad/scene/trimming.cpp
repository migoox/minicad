#include <liberay/util/panic.hpp>
#include <libminicad/scene/trimming.hpp>
#include <vector>

namespace mini {

ParamSpaceTrimmingData ParamSpaceTrimmingData::from_intersection_curve(
    ISceneRenderer& renderer, const IntersectionFinder::ParamSpace& param_space) {
  auto trimming_txt_vis = std::vector<uint32_t>();

  trimming_txt_vis.resize(IntersectionFinder::Curve::kTxtSize * IntersectionFinder::Curve::kTxtSize, 0xFFFFFFFF);
  for (auto j = 0U; j < IntersectionFinder::Curve::kTxtSize; ++j) {
    for (auto i = 0U; i < IntersectionFinder::Curve::kTxtSize; ++i) {
      auto idx = j * IntersectionFinder::Curve::kTxtSize + i;

      if (param_space.trimming_txt1[idx] == 0xFF000000) {
        trimming_txt_vis[idx] = 0xFF000000;
      } else {
        trimming_txt_vis[idx] = 0xFFFFFFFF;
      }

      if (param_space.curve_txt[idx] == 0xFF000000) {
        trimming_txt_vis[idx] = 0xFF0000FF;
      }
    }
  }
  auto th1 = renderer.upload_texture(trimming_txt_vis, IntersectionFinder::Curve::kTxtSize,
                                     IntersectionFinder::Curve::kTxtSize);

  for (auto j = 0U; j < IntersectionFinder::Curve::kTxtSize; ++j) {
    for (auto i = 0U; i < IntersectionFinder::Curve::kTxtSize; ++i) {
      auto idx = j * IntersectionFinder::Curve::kTxtSize + i;

      if (param_space.trimming_txt2[idx] == 0xFF000000) {
        trimming_txt_vis[idx] = 0xFF000000;
      } else {
        trimming_txt_vis[idx] = 0xFFFFFFFF;
      }

      if (param_space.curve_txt[idx] == 0xFF000000) {
        trimming_txt_vis[idx] = 0xFF0000FF;
      }
    }
  }
  auto th2 = renderer.upload_texture(trimming_txt_vis, IntersectionFinder::Curve::kTxtSize,
                                     IntersectionFinder::Curve::kTxtSize);

  return ParamSpaceTrimmingData{
      .curve_txt = renderer.upload_texture(param_space.curve_txt, IntersectionFinder::Curve::kTxtSize,
                                           IntersectionFinder::Curve::kTxtSize),
      .trimming_variant_txt =
          {
              th1,
              th2,
          },
      .trimming_variant_txt_data =
          {
              std::move(param_space.trimming_txt1),
              std::move(param_space.trimming_txt2),
          },
      .enable = false,
  };
}

ParamSpaceTrimmingDataManager ParamSpaceTrimmingDataManager::create(size_t width, size_t height) {
  auto ps = ParamSpaceTrimmingDataManager(width, height);
  ps.update_final_txt(true);
  ps.final_txt_.resize(width * height, 0xFFFFFFFF);
  return ps;
}

void ParamSpaceTrimmingDataManager::add(ParamSpaceTrimmingData&& data) {
  if (data.trimming_variant_txt_data[0].size() != width_ * height_) {
    eray::util::panic("Trimming texture size does not match the trimming data manager expected size");
  }
  if (data.trimming_variant_txt_data[1].size() != width_ * height_) {
    eray::util::panic("Trimming texture size does not match the trimming data manager expected size");
  }
  dirty_ = true;
  data_.emplace_back(std::move(data));
}

void ParamSpaceTrimmingDataManager::remove(uint32_t idx) {
  dirty_ = true;
  data_.erase(data_.begin() + idx);
}

void ParamSpaceTrimmingDataManager::update_final_txt(bool force) {
  if (!dirty_ && !force) {
    return;
  }
  dirty_ = false;

  std::ranges::fill(final_txt_, 0xFFFFFFFF);

  for (const auto& d : data_) {
    if (!d.enable) {
      continue;
    }

    for (auto j = 0U; j < height_; ++j) {
      for (auto i = 0U; i < width_; ++i) {
        auto idx = j * width_ + i;
        if (d.trimming_variant_txt_data[d.reverse ? 1 : 0][idx] == 0xFF000000) {
          final_txt_[idx] = 0xFF000000;
        }
      }
    }
  }
}

const std::vector<uint32_t>& ParamSpaceTrimmingDataManager::final_txt() {
  update_final_txt();
  return final_txt_;
}

}  // namespace mini
