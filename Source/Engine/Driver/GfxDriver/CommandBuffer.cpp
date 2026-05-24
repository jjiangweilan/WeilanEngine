#include "CommandBuffer.hpp"

namespace Gfx
{
void CopyDataToImage(uint8_t* data, Gfx::Image& dst, int width, int height, int mipLevels, int layers)
{
}

DescriptorBinding::DescriptorBinding(int dstBinding, ImageView* imageView)
    : dstBinding(dstBinding),
      dstArrayElement(0),
      descriptorCount(1),
      imageView(imageView),
      buffer(nullptr) {}

DescriptorBinding::DescriptorBinding(int dstBinding, BufferIdentifier buffer)
    : dstBinding(dstBinding),
      dstArrayElement(0),
      descriptorCount(1),
      imageView(nullptr),
      buffer(buffer) {}

DescriptorBinding::DescriptorBinding(int dstBinding, Image* image)
    : dstBinding(dstBinding),
      dstArrayElement(0),
      descriptorCount(1),
      imageView(&image->GetDefaultImageView()),
      buffer(nullptr) {}
} // namespace Gfx
