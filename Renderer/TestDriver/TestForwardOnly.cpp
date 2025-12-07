#include "CoreLib/Basic.h"
#include "IRasterRenderer.h"
#include "TestScene.h"
#include "ViewSettings.h"
#include "ForwardLightingShader.h"
#include "CoreLib/PerformanceCounter.h"
#include <iostream>

using namespace CoreLib::Basic;
using namespace CoreLib::Diagnostics;
using namespace RasterRenderer;
using namespace Testing;
using namespace VectorMath;

int main(int argc, char* argv[])
{
    int width = 1024;
    int height = 768;
    
    ViewSettings viewSettings;
    viewSettings.WindowWidth = width;
    viewSettings.WindowHeight = height;
    viewSettings.FovY = 45.0f;
    viewSettings.zNear = 0.1f;
    viewSettings.zFar = 1000.0f;
    
    FrameBuffer frameBuffer(width, height);
    IRasterRenderer* renderer = CreateTiledRenderer();
    renderer->SetFrameBuffer(&frameBuffer);
    
    // Create triangle scene (no media files needed)
    RefPtr<TestScene> scene = CreateTestScene0(viewSettings);
    
    // Set up forward lighting shader
    ForwardLightingShader* shader = new ForwardLightingShader();
    shader->CameraPosition = Vec3(0.0f, 0.0f, 10.0f);
    shader->Shininess = 32.0f;
    shader->SpecularColor = Vec3(0.5f, 0.5f, 0.5f);
    
    // Add one light
    ForwardLightingShader::Light light;
    light.LightType = ForwardLightingShader::Light::DIRECTIONAL;
    light.Direction = Vec3(0.0f, -1.0f, -1.0f);
    light.Color = Vec3(1.0f, 1.0f, 0.95f);
    light.Intensity = 2.0f;
    light.Ambient = 0.2f;
    shader->Lights.Add(light);
    
    scene->SetShader(shader);
    
    printf("Testing forward renderer with 1 light...\n");
    
    // Warmup
    renderer->Clear(Vec4(0, 0, 0, 0));
    scene->Draw(renderer);
    renderer->Finish();
    
    // Benchmark
    const int frameCount = 10;
    double minTime = 1e10;
    for (int i = 0; i < frameCount; i++)
    {
        auto counter = PerformanceCounter::Start();
        renderer->Clear(scene->ClearColor);
        scene->Draw(renderer);
        renderer->Finish();
        auto elapsed = PerformanceCounter::ToSeconds(PerformanceCounter::End(counter));
        minTime = (elapsed < minTime) ? elapsed : minTime;
    }
    
    printf("Forward renderer: %.2f ms/frame (%.1f FPS)\n", minTime * 1000.0, 1.0 / minTime);
    
    DestroyRenderer(renderer);
    return 0;
}

