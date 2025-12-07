#ifndef NFL_SCENE_H
#define NFL_SCENE_H

#include "CoreLib/Basic.h"
#include "TestScene.h"
#include "NFLTrackingData.h"
#include "ModelResource.h"
#include "CoreLib/VectorMath.h"
#include "CoreLib/Graphics/ObjModel.h"
#include <vector>
#include <map>

using namespace CoreLib::Basic;
using namespace CoreLib::Graphics;
using namespace RasterRenderer;
using namespace Testing;
using namespace VectorMath;
using namespace NFL;

// Simple player model (a colored box)
class SimplePlayerModel
{
private:
    ModelResource model;
    
public:
    SimplePlayerModel(const Vec3& color);
    void Draw(RenderState& state, IRasterRenderer* renderer);
    void SetShader(Shader* shader);
};

// NFL play scene with stadium and animated players
class NFLPlayScene : public TestScene
{
private:
    ModelResource stadiumModel;
    std::vector<SimplePlayerModel> playerModels;
    std::map<int, std::vector<PlayerPosition>> playerPositions;
    std::vector<int> steps;
    int currentStep;
    
    Vec3 GetTeamColor(const String& team);
    
public:
    NFLPlayScene(ViewSettings& viewSettings, const String& stadiumModelPath, const PlayData& playData);
    void SetStep(int step);
    virtual void Draw(IRasterRenderer* renderer);
    virtual void SetShader(Shader* shader);
};

#endif

