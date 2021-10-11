#include <ATen/native/vulkan/ops/Common.h>
#include <ATen/native/vulkan/api/Helper.h>
#include <torch/library.h>

namespace at {
namespace native {
namespace vulkan {
namespace ops {
namespace {

using namespace api::utils;

namespace {
inline int64_t normalize_dim(int64_t d, int64_t n) {
  return (d % n + n) % n;
}
} // namespace

Tensor cat_batch(const TensorList tensors, vTensor& v_output) {
  TORCH_CHECK(false, "Vulkan cat not implemented for batch dimension!");
}

Tensor cat_feature(const TensorList tensors, vTensor& v_output) {
  TORCH_CHECK(false, "Vulkan cat not implemented for feature dimension!");
}

Tensor cat_width(const TensorList tensors, vTensor& v_output) {
  TORCH_CHECK(false, "Vulkan cat not implemented for width dimension!");
}

Tensor cat_height(const TensorList tensors, vTensor& v_output) {
  api::Context* const context = api::context();
  api::Command::Pool& command_pool = context->command().pool;
  api::Command::Buffer& command_buffer = command_pool.stream();

  auto dst_image = v_output.image(
    command_buffer,
    vTensor::Stage::Compute,
    vTensor::Access::Write);

  uvec3 dst_offset{};
  for (const auto& tensor : tensors) {
    const Tensor self = tensor.is_vulkan() ? tensor : tensor.vulkan();
    const vTensor& v_self = convert(self);
    if C10_LIKELY(v_output.has_image() && v_self.has_image()) {
      auto src_image = v_self.image(
              command_buffer,
              vTensor::Stage::Compute);

      api::helper::copy_texture_to_texture(command_buffer,
        src_image,
        dst_image,
        v_self.extents(),
        dst_offset);

      // Increment by height
      dst_offset.data[1u] += v_self.extents().data[1u];
    }
    else {
      TORCH_CHECK(false, "Not implemented!");
    }
  }

  command_pool.submit(context->gpu().queue, command_buffer);

  return convert(v_output);
}

Tensor cat(
  const at::TensorList tensors,
  const int64_t dim) {
  const auto norm_dim = normalize_dim(dim, 4);
  TORCH_CHECK(
    tensors.size() > 0,
    "Vulkan cat expects at least one tensor");

  at::Tensor tensor = tensors[0];
  int64_t cat_dim_size = 0;

  for (int i = 0; i < tensors.size(); ++i) {
    const auto& t = tensors[i];
    TORCH_INTERNAL_ASSERT(
      t.dim() == 4, "Vulkan cat expects 4 dimensional inputs");

    for (int d = 0; d < 4; ++d) {
      if (d == dim) {
        continue;
      }
      TORCH_INTERNAL_ASSERT(
        t.size(d) == tensor.size(d),
        "Vulkan cat inputs must have matching sizes except concatenated dimension");
    }
    cat_dim_size += t.size(dim);
  }

  auto result_size = tensor.sizes().vec();
  result_size[dim] = cat_dim_size;

  vTensor v_output{
    api::context(),
    result_size,
    tensor.options()};

  if (dim == 3) {
    return cat_width(tensors, v_output);
  }
  if (dim == 2) {
    return cat_height(tensors, v_output);
  }
  else if (dim == 1) {
    return cat_feature(tensors, v_output);
  }
  return cat_batch(tensors, v_output);
}

#ifdef USE_VULKAN_API

TORCH_LIBRARY_IMPL(aten, Vulkan, m) {
  m.impl("_cat", TORCH_FN(cat));
}

#endif /* USE_VULKAN_API */

} // namespace
} // namespace ops
} // namespace vulkan
} // namespace native
} // namespace at
