#include <liberay/util/panic.hpp>
#include <libminicad/scene/trimming.hpp>
#include <vector>

namespace mini {

ParamSpaceTrimmingData ParamSpaceTrimmingData::from_intersection_curve(
    ISceneRenderer& renderer, const IntersectionFinder::ParamSpace& param_space) {
  constexpr auto kCPUTrimmingTxtSizeFlt = static_cast<float>(kCPUTrimmingTxtSize);
  constexpr auto kGPUTrimmingTxtSizeFlt = static_cast<float>(kGPUTrimmingTxtSize);

  auto trimming_txt_vis = std::vector<uint32_t>();
  trimming_txt_vis.resize(kGPUTrimmingTxtSize * kGPUTrimmingTxtSize);

  for (auto i = 0U; i < kCPUTrimmingTxtSize; ++i) {
    for (auto j = 0U; j < kCPUTrimmingTxtSize; ++j) {
      const auto idx = i * kCPUTrimmingTxtSize + j;

      const auto mapped_i =
          static_cast<size_t>(static_cast<float>(i) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);
      const auto mapped_j =
          static_cast<size_t>(static_cast<float>(j) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);

      const auto mapped_idx = mapped_i * kGPUTrimmingTxtSize + mapped_j;

      if (param_space.trimming_txt1[idx] == 0xFF000000) {
        trimming_txt_vis[mapped_idx] = 0xFF000000;
      } else {
        trimming_txt_vis[mapped_idx] = 0xFFFFFFFF;
      }
    }
  }

  for (auto i = 0U; i < kCPUTrimmingTxtSize; ++i) {
    for (auto j = 0U; j < kCPUTrimmingTxtSize; ++j) {
      const auto idx = i * kCPUTrimmingTxtSize + j;

      const auto mapped_i =
          static_cast<size_t>(static_cast<float>(i) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);
      const auto mapped_j =
          static_cast<size_t>(static_cast<float>(j) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);

      const auto mapped_idx = mapped_i * kGPUTrimmingTxtSize + mapped_j;

      if (param_space.curve_txt[idx] == 0xFF000000) {
        trimming_txt_vis[mapped_idx] = 0xFF0000FF;
      }
    }
  }

  auto th1 = renderer.upload_texture(trimming_txt_vis, kGPUTrimmingTxtSize, kGPUTrimmingTxtSize);

  for (auto i = 0U; i < kCPUTrimmingTxtSize; ++i) {
    for (auto j = 0U; j < kCPUTrimmingTxtSize; ++j) {
      const auto idx = i * kCPUTrimmingTxtSize + j;

      const auto mapped_i =
          static_cast<size_t>(static_cast<float>(i) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);
      const auto mapped_j =
          static_cast<size_t>(static_cast<float>(j) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);

      const auto mapped_idx = mapped_i * kGPUTrimmingTxtSize + mapped_j;

      if (param_space.trimming_txt2[idx] == 0xFF000000) {
        trimming_txt_vis[mapped_idx] = 0xFF000000;
      } else {
        trimming_txt_vis[mapped_idx] = 0xFFFFFFFF;
      }
    }
  }
  for (auto i = 0U; i < kCPUTrimmingTxtSize; ++i) {
    for (auto j = 0U; j < kCPUTrimmingTxtSize; ++j) {
      const auto idx = i * kCPUTrimmingTxtSize + j;

      const auto mapped_i =
          static_cast<size_t>(static_cast<float>(i) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);
      const auto mapped_j =
          static_cast<size_t>(static_cast<float>(j) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);

      const auto mapped_idx = mapped_i * kGPUTrimmingTxtSize + mapped_j;

      if (param_space.curve_txt[idx] == 0xFF000000) {
        trimming_txt_vis[mapped_idx] = 0xFF0000FF;
      }
    }
  }
  auto th2 = renderer.upload_texture(trimming_txt_vis, kGPUTrimmingTxtSize, kGPUTrimmingTxtSize);

  return ParamSpaceTrimmingData{
      .curve_txt = renderer.upload_texture(param_space.curve_txt, kGPUTrimmingTxtSize, kGPUTrimmingTxtSize),
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

ParamSpaceTrimmingDataManager ParamSpaceTrimmingDataManager::create() {
  auto ps = ParamSpaceTrimmingDataManager();
  ps.final_txt_.resize(kCPUTrimmingTxtSize * kCPUTrimmingTxtSize, 0xFFFFFFFF);
  ps.final_txt_gpu_.resize(kGPUTrimmingTxtSize * kGPUTrimmingTxtSize, 0xFFFFFFFF);
  ps.update_final_txt(true);
  return ps;
}

void ParamSpaceTrimmingDataManager::add(ParamSpaceTrimmingData&& data) {
  if (data.trimming_variant_txt_data[0].size() != kCPUTrimmingTxtSize * kCPUTrimmingTxtSize) {
    eray::util::panic("Trimming texture size does not match the trimming data manager expected size");
  }
  if (data.trimming_variant_txt_data[1].size() != kCPUTrimmingTxtSize * kCPUTrimmingTxtSize) {
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

    for (auto i = 0U; i < kCPUTrimmingTxtSize; ++i) {
      for (auto j = 0U; j < kCPUTrimmingTxtSize; ++j) {
        auto idx = i * kCPUTrimmingTxtSize + j;
        if (d.trimming_variant_txt_data[d.reverse ? 1 : 0][idx] == 0xFF000000) {
          final_txt_[idx] = 0xFF000000;
        }
      }
    }
  }
  constexpr auto kCPUTrimmingTxtSizeFlt = static_cast<float>(kCPUTrimmingTxtSize);
  constexpr auto kGPUTrimmingTxtSizeFlt = static_cast<float>(kGPUTrimmingTxtSize);

  for (auto i = 0U; i < kCPUTrimmingTxtSize; ++i) {
    for (auto j = 0U; j < kCPUTrimmingTxtSize; ++j) {
      const auto idx = i * kCPUTrimmingTxtSize + j;

      const auto mapped_i =
          static_cast<size_t>(static_cast<float>(i) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);
      const auto mapped_j =
          static_cast<size_t>(static_cast<float>(j) / kCPUTrimmingTxtSizeFlt * kGPUTrimmingTxtSizeFlt);

      const auto mapped_idx      = mapped_i * kGPUTrimmingTxtSize + mapped_j;
      final_txt_gpu_[mapped_idx] = final_txt_[idx];
    }
  }
}

const std::vector<uint32_t>& ParamSpaceTrimmingDataManager::final_txt() {
  update_final_txt();
  return final_txt_;
}

const std::vector<uint32_t>& ParamSpaceTrimmingDataManager::final_txt_gpu() {
  update_final_txt();
  return final_txt_gpu_;
}

}  // namespace mini
