#pragma once
#include "../Node.hpp"
#include "Core/GameScene/GameScene.hpp"

namespace Engine::RGraph
{
class DrawNode : public Node
{
public:
    DrawNode();

    bool Preprocess(ResourceStateTrack& stateTrack) override;
    bool Compile(ResourceStateTrack& stateTrack) override;
    bool Execute(ResourceStateTrack& stateTrack) override;

    void SetGameScene(GameScene* gameScnee);

private:
    Port* renderPassIn;
    Port* renderPassOut;
    GameScene* gameScene;
};
} // namespace Engine::RGraph
