#pragma once
#include <cinttypes>

namespace Engine::Gfx
{
    typedef uint32_t DescriptorSetSlot;
    constexpr uint32_t Descriptor_Set_Count = 4;

    constexpr DescriptorSetSlot Scene_Descriptor_Set = 0;
    constexpr DescriptorSetSlot Shader_Descriptor_Set = 1;
    constexpr DescriptorSetSlot Material_Descriptor_Set = 2;
    constexpr DescriptorSetSlot Object_Descriptor_Set = 3;
}

