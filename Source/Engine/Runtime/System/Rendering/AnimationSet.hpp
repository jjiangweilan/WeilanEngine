#pragma once

#include "Engine/Core/Asset.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/System/Rendering/AnimationClip.hpp"
#include <string_view>
#include <vector>

class AnimationSet : public Asset
{
    DECLARE_ASSET();

public:
    using Clips = std::vector<ObjPtr<AnimationClip>>;

    const Clips& GetClips() const { return clips; }

    void AddClip(AnimationClip* clip)
    {
        clips.push_back(clip);
    }

    AnimationClip* FindClip(std::string_view name) const
    {
        for (const ObjPtr<AnimationClip>& clip : clips)
        {
            if (clip && clip->GetName() == name)
                return clip;
        }

        return nullptr;
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    Clips clips;
};
