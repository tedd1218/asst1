#include "CoreLib/Basic.h"
#include "IRasterRenderer.h"
#include "ViewSettings.h"
#include "ForwardLightingShader.h"
#include "NFLTrackingData.h"
#include "NFLScene.h"
#include "CoreLib/PerformanceCounter.h"
#include "CoreLib/LibIO.h"
#include "CoreLib/VectorMath.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <fstream>
#include <map>
#include <algorithm>
#include <cstring>

using namespace CoreLib::Basic;
using namespace CoreLib::Diagnostics;
using namespace CoreLib::Graphics;
using namespace CoreLib::IO;
using namespace RasterRenderer;
using namespace Testing;
using namespace VectorMath;
using namespace NFL;

struct NFLBenchmarkResult
{
    String rendererName;
    int frameCount;
    int lightCount;
    double totalTimeMs;
    double avgFrameTimeMs;
    double fps;
    double minFrameTimeMs;
    double maxFrameTimeMs;
};

struct LightScalingResult
{
    int lightCount;
    double forwardFps;
    double deferredFps;
    double forwardTimeMs;
    double deferredTimeMs;
    String winner;
    double speedup; // deferred/forward time ratio (< 1 means deferred is faster)
};

class NFLRendererComparison
{
private:
    int width, height;
    ViewSettings viewSettings;
    String stadiumModelPath;
    PlayData playData;
    RefPtr<NFLPlayScene> forwardScene;
    RefPtr<NFLPlayScene> deferredScene;
    
    void SetupLights(ForwardLightingShader* shader)
    {
        // Default setup with 5 lights
        SetupLightsWithCount(shader, 5);
    }
    
    void SetupLightsWithCount(ForwardLightingShader* shader, int numLights)
    {
        shader->Lights.Clear();
        
        // Always add at least one directional light (sun)
        ForwardLightingShader::Light sunLight;
        sunLight.LightType = ForwardLightingShader::Light::DIRECTIONAL;
        sunLight.Direction = Vec3(0.0f, 0.0f, -1.0f);
        sunLight.Color = Vec3(1.0f, 1.0f, 0.95f);
        sunLight.Intensity = 4.0f;
        sunLight.Ambient = 0.2f;  // Lower ambient for more lights
        shader->Lights.Add(sunLight);
        
        int pointLightsNeeded = numLights - 1;
        if (pointLightsNeeded <= 0) return;
        
        // Stadium field is roughly from x=[0,120] y=[0,53.3] (yards)
        float fieldMinX = -10.0f;
        float fieldMaxX = 130.0f;
        float fieldMinY = -10.0f;
        float fieldMaxY = 63.0f;
        float lightHeight = 50.0f;
        
        // Calculate grid dimensions
        int gridCols = (int)ceil(sqrt((double)pointLightsNeeded * 1.5));
        int gridRows = (int)ceil((double)pointLightsNeeded / gridCols);
        
        float stepX = (fieldMaxX - fieldMinX) / (gridCols + 1);
        float stepY = (fieldMaxY - fieldMinY) / (gridRows + 1);
        
        // Adjust light intensity based on count - more lights = less intensity each
        float intensityPerLight = 800.0f / sqrt((float)pointLightsNeeded);
        
        int lightsAdded = 0;
        for (int row = 0; row < gridRows && lightsAdded < pointLightsNeeded; row++)
        {
            for (int col = 0; col < gridCols && lightsAdded < pointLightsNeeded; col++)
            {
                ForwardLightingShader::Light pointLight;
                pointLight.LightType = ForwardLightingShader::Light::POINT;
                pointLight.Position = Vec3(
                    fieldMinX + stepX * (col + 1),
                    fieldMinY + stepY * (row + 1),
                    lightHeight + (float)((row + col) % 3) * 5.0f  // Slight height variation
                );
                
                // Vary colors slightly for visual interest
                float r = 1.0f;
                float g = 1.0f - (float)(lightsAdded % 3) * 0.05f;
                float b = 0.95f - (float)(lightsAdded % 5) * 0.05f;
                pointLight.Color = Vec3(r, g, b);
                
                pointLight.Intensity = intensityPerLight;
                pointLight.Ambient = 0.05f;
                pointLight.Decay = 80.0f;
                
                shader->Lights.Add(pointLight);
                lightsAdded++;
            }
        }
    }
    
public:
    NFLRendererComparison(int width, int height, const String& stadiumModelPath, const PlayData& playData)
        : width(width), height(height), stadiumModelPath(stadiumModelPath), playData(playData),
          forwardScene(nullptr), deferredScene(nullptr)
    {
        viewSettings.WindowWidth = width;
        viewSettings.WindowHeight = height;
        viewSettings.FovY = 45.0f;
        viewSettings.zNear = 0.1f;
        viewSettings.zFar = 1000.0f;
    }
    
    ~NFLRendererComparison()
    {
        // Explicitly clear scenes in destructor to control destruction order
        printf("NFLRendererComparison destructor: clearing scenes...\n");
        fflush(stdout);
        forwardScene = nullptr;
        deferredScene = nullptr;
        printf("NFLRendererComparison destructor: scenes cleared.\n");
        fflush(stdout);
    }
    
    NFLBenchmarkResult BenchmarkForward()
    {
        printf("\n=== Benchmarking Forward Renderer ===\n");
        FrameBuffer frameBuffer(width, height);
        IRasterRenderer* renderer = CreateTiledRenderer();
        renderer->SetFrameBuffer(&frameBuffer);
        
        // Create NFL scene
        forwardScene = new NFLPlayScene(viewSettings, stadiumModelPath, playData);
        
        // Set up forward lighting shader
        ForwardLightingShader* shader = new ForwardLightingShader();
        shader->CameraPosition = Vec3(60.0f, 60.0f, 50.0f);
        shader->Shininess = 32.0f;
        shader->SpecularColor = Vec3(0.5f, 0.5f, 0.5f);
        SetupLights(shader);
        forwardScene->SetShader(shader);
        
        // Warmup
        forwardScene->SetStep(playData.steps[0]);
        renderer->Clear(forwardScene->ClearColor);
        forwardScene->Draw(renderer);
        renderer->Finish();
        
        // Benchmark
        double minTime = 1e10;
        double maxTime = 0.0;
        double totalTime = 0.0;
        int frameCount = (int)playData.steps.size();
        
        for (int i = 0; i < frameCount; i++)
        {
            int step = playData.steps[i];
            forwardScene->SetStep(step);
            
            auto counter = PerformanceCounter::Start();
            renderer->Clear(forwardScene->ClearColor);
            forwardScene->Draw(renderer);
            renderer->Finish();
            auto elapsed = PerformanceCounter::End(counter);
            double frameTime = PerformanceCounter::ToSeconds(elapsed) * 1000.0; // Convert to milliseconds
            
            totalTime += frameTime;
            if (frameTime < minTime) minTime = frameTime;
            if (frameTime > maxTime) maxTime = frameTime;
            
            if ((i + 1) % 10 == 0 || i == 0)
            {
                printf("  Frame %d/%d: %.2f ms\n", i + 1, frameCount, frameTime);
            }
        }
        
        NFLBenchmarkResult result;
        result.rendererName = L"Forward";
        result.frameCount = frameCount;
        result.totalTimeMs = totalTime;
        result.avgFrameTimeMs = totalTime / frameCount;
        result.fps = 1000.0 / result.avgFrameTimeMs;
        result.minFrameTimeMs = minTime;
        result.maxFrameTimeMs = maxTime;
        
        printf("  Total: %.2f ms (%.2f s)\n", totalTime, totalTime / 1000.0);
        printf("  Average: %.2f ms/frame\n", result.avgFrameTimeMs);
        printf("  FPS: %.2f\n", result.fps);
        printf("  Min: %.2f ms, Max: %.2f ms\n", minTime, maxTime);
        
        printf("  Cleaning up forward renderer...\n");
        fflush(stdout);
        
        // Destroy renderer first
        DestroyRenderer(renderer);
        renderer = nullptr;
        
        printf("  Forward renderer destroyed.\n");
        fflush(stdout);
        
        // Copy result before any cleanup
        NFLBenchmarkResult returnResult = result;
        
        printf("  Result copied successfully.\n");
        fflush(stdout);
        
        printf("  About to return from BenchmarkForward()...\n");
        fflush(stdout);
        
        return returnResult;
    }
    
    NFLBenchmarkResult BenchmarkDeferred()
    {
        printf("\n=== Benchmarking Deferred Renderer ===\n");
        fflush(stdout);
        
        // Validate playData before proceeding
        if (playData.steps.size() == 0)
        {
            printf("ERROR: playData is invalid (no steps). Skipping deferred benchmark.\n");
            fflush(stdout);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        }
        
        printf("  Validated playData: %d steps, %d players\n", (int)playData.steps.size(), (int)playData.players.size());
        fflush(stdout);
        
        printf("  Creating frame buffer...\n");
        fflush(stdout);
        FrameBuffer frameBuffer(width, height);
        
        printf("  Attempting to create deferred renderer...\n");
        fflush(stdout);
        
        // Try to create deferred renderer
        IRasterRenderer* renderer = nullptr;
        try {
            printf("  Calling CreateDeferredTiledRenderer()...\n");
            fflush(stdout);
            renderer = CreateDeferredTiledRenderer();
            printf("  CreateDeferredTiledRenderer() returned: %p\n", (void*)renderer);
            fflush(stdout);
        } catch (Exception& ex) {
            printf("ERROR: Exception creating deferred renderer: %s\n", ex.Message.ToMultiByteString());
            fflush(stdout);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        } catch (...) {
            printf("ERROR: Unknown exception creating deferred renderer. Skipping deferred benchmark.\n");
            fflush(stdout);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        }
        
        if (!renderer) {
            printf("ERROR: Could not create deferred renderer (returned nullptr). Skipping deferred benchmark.\n");
            fflush(stdout);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        }
        
        printf("  Deferred renderer created successfully.\n");
        fflush(stdout);
        
        printf("  Setting frame buffer on renderer...\n");
        fflush(stdout);
        try {
            renderer->SetFrameBuffer(&frameBuffer);
            printf("  Frame buffer set successfully.\n");
            fflush(stdout);
        } catch (...) {
            printf("ERROR: Exception setting frame buffer. Cleaning up and skipping.\n");
            fflush(stdout);
            DestroyRenderer(renderer);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        }
        
        printf("  Creating NFL scene for deferred renderer...\n");
        fflush(stdout);
        // Create NFL scene
        try {
            deferredScene = new NFLPlayScene(viewSettings, stadiumModelPath, playData);
            printf("  NFL scene created successfully.\n");
            fflush(stdout);
        } catch (Exception& ex) {
            printf("ERROR: Exception creating scene: %s\n", ex.Message.ToMultiByteString());
            fflush(stdout);
            DestroyRenderer(renderer);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        } catch (...) {
            printf("ERROR: Unknown exception creating scene. Cleaning up and skipping.\n");
            fflush(stdout);
            DestroyRenderer(renderer);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        }
        
        // Set up forward lighting shader (deferred renderer uses same light structure)
        printf("  Creating shader for deferred renderer...\n");
        fflush(stdout);
        ForwardLightingShader* shader = new ForwardLightingShader();
        printf("  Shader created.\n");
        fflush(stdout);
        
        printf("  Setting shader properties...\n");
        fflush(stdout);
        shader->CameraPosition = Vec3(60.0f, 60.0f, 50.0f);
        shader->Shininess = 32.0f;
        shader->SpecularColor = Vec3(0.5f, 0.5f, 0.5f);
        
        printf("  Setting up lights...\n");
        fflush(stdout);
        SetupLights(shader);
        
        printf("  Setting shader on scene...\n");
        fflush(stdout);
        try {
            deferredScene->SetShader(shader);
            printf("  Shader set on scene successfully.\n");
            fflush(stdout);
        } catch (...) {
            printf("  ERROR: Exception setting shader on scene.\n");
            fflush(stdout);
            DestroyRenderer(renderer);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        }
        
        // Warmup
        printf("  Starting warmup render...\n");
        fflush(stdout);
        try {
            deferredScene->SetStep(playData.steps[0]);
            printf("  Step set.\n");
            fflush(stdout);
            renderer->Clear(deferredScene->ClearColor);
            printf("  Cleared renderer.\n");
            fflush(stdout);
            printf("  About to call deferredScene->Draw(renderer)...\n");
            fflush(stdout);
            try {
                deferredScene->Draw(renderer);
                printf("  Drew scene successfully.\n");
                fflush(stdout);
            } catch (...) {
                printf("  ERROR: Exception during scene->Draw().\n");
                fflush(stdout);
                DestroyRenderer(renderer);
                NFLBenchmarkResult result;
                result.rendererName = L"Deferred";
                result.frameCount = 0;
                result.totalTimeMs = 0;
                result.avgFrameTimeMs = 0;
                result.fps = 0;
                result.minFrameTimeMs = 0;
                result.maxFrameTimeMs = 0;
                return result;
            }
            
            printf("  About to call renderer->Finish()...\n");
            fflush(stdout);
            try {
                renderer->Finish();
                printf("  Finished renderer successfully.\n");
                fflush(stdout);
            } catch (...) {
                printf("  ERROR: Exception during renderer->Finish().\n");
                fflush(stdout);
                DestroyRenderer(renderer);
                NFLBenchmarkResult result;
                result.rendererName = L"Deferred";
                result.frameCount = 0;
                result.totalTimeMs = 0;
                result.avgFrameTimeMs = 0;
                result.fps = 0;
                result.minFrameTimeMs = 0;
                result.maxFrameTimeMs = 0;
                return result;
            }
        } catch (...) {
            printf("  ERROR: Exception during warmup render.\n");
            fflush(stdout);
            DestroyRenderer(renderer);
            NFLBenchmarkResult result;
            result.rendererName = L"Deferred";
            result.frameCount = 0;
            result.totalTimeMs = 0;
            result.avgFrameTimeMs = 0;
            result.fps = 0;
            result.minFrameTimeMs = 0;
            result.maxFrameTimeMs = 0;
            return result;
        }
        
        // Benchmark
        double minTime = 1e10;
        double maxTime = 0.0;
        double totalTime = 0.0;
        int frameCount = (int)playData.steps.size();
        
        for (int i = 0; i < frameCount; i++)
        {
            int step = playData.steps[i];
            deferredScene->SetStep(step);
            
            auto counter = PerformanceCounter::Start();
            renderer->Clear(deferredScene->ClearColor);
            deferredScene->Draw(renderer);
            renderer->Finish();
            auto elapsed = PerformanceCounter::End(counter);
            double frameTime = PerformanceCounter::ToSeconds(elapsed) * 1000.0; // Convert to milliseconds
            
            totalTime += frameTime;
            if (frameTime < minTime) minTime = frameTime;
            if (frameTime > maxTime) maxTime = frameTime;
            
            if ((i + 1) % 10 == 0 || i == 0)
            {
                printf("  Frame %d/%d: %.2f ms\n", i + 1, frameCount, frameTime);
            }
        }
        
        NFLBenchmarkResult result;
        result.rendererName = L"Deferred";
        result.frameCount = frameCount;
        result.totalTimeMs = totalTime;
        result.avgFrameTimeMs = totalTime / frameCount;
        result.fps = 1000.0 / result.avgFrameTimeMs;
        result.minFrameTimeMs = minTime;
        result.maxFrameTimeMs = maxTime;
        
        printf("  Total: %.2f ms (%.2f s)\n", totalTime, totalTime / 1000.0);
        printf("  Average: %.2f ms/frame\n", result.avgFrameTimeMs);
        printf("  FPS: %.2f\n", result.fps);
        printf("  Min: %.2f ms, Max: %.2f ms\n", minTime, maxTime);
        
        printf("  Cleaning up deferred renderer...\n");
        fflush(stdout);
        
        // Destroy renderer
        DestroyRenderer(renderer);
        renderer = nullptr;  // Set to nullptr to avoid use-after-free
        
        printf("  Deferred renderer destroyed.\n");
        fflush(stdout);
        
        return result;
    }
    
    void GenerateComparisonReport(const NFLBenchmarkResult& forward, const NFLBenchmarkResult& deferred, const String& outputPath)
    {
        std::ofstream file(outputPath.ToMultiByteString());
        if (!file.is_open())
        {
            printf("ERROR: Could not open output file: %s\n", outputPath.ToMultiByteString());
            return;
        }
        
        file << "# NFL Renderer Comparison Report\n";
        file << "# Generated automatically\n\n";
        
        file << "## Configuration\n";
        file << "- Resolution: " << width << "x" << height << "\n";
        file << "- Frames: " << forward.frameCount << "\n";
        file << "- Lights: 5 (1 directional + 4 point lights)\n\n";
        
        file << "## Results\n\n";
        file << "| Metric | Forward | Deferred | Winner |\n";
        file << "|--------|---------|----------|--------|\n";
        
        // Total time
        file << "| Total Time (s) | " << std::fixed << std::setprecision(2) 
             << forward.totalTimeMs / 1000.0 << " | " 
             << (deferred.frameCount > 0 ? deferred.totalTimeMs / 1000.0 : 0.0) << " | ";
        if (deferred.frameCount > 0 && deferred.totalTimeMs < forward.totalTimeMs)
            file << "Deferred |\n";
        else
            file << "Forward |\n";
        
        // Average frame time
        file << "| Avg Frame Time (ms) | " << forward.avgFrameTimeMs << " | " 
             << (deferred.frameCount > 0 ? deferred.avgFrameTimeMs : 0.0) << " | ";
        if (deferred.frameCount > 0 && deferred.avgFrameTimeMs < forward.avgFrameTimeMs)
            file << "Deferred |\n";
        else
            file << "Forward |\n";
        
        // FPS
        file << "| FPS | " << forward.fps << " | " 
             << (deferred.frameCount > 0 ? deferred.fps : 0.0) << " | ";
        if (deferred.frameCount > 0 && deferred.fps > forward.fps)
            file << "Deferred |\n";
        else
            file << "Forward |\n";
        
        // Min frame time
        file << "| Min Frame Time (ms) | " << forward.minFrameTimeMs << " | " 
             << (deferred.frameCount > 0 ? deferred.minFrameTimeMs : 0.0) << " | ";
        if (deferred.frameCount > 0 && deferred.minFrameTimeMs < forward.minFrameTimeMs)
            file << "Deferred |\n";
        else
            file << "Forward |\n";
        
        // Max frame time
        file << "| Max Frame Time (ms) | " << forward.maxFrameTimeMs << " | " 
             << (deferred.frameCount > 0 ? deferred.maxFrameTimeMs : 0.0) << " | ";
        if (deferred.frameCount > 0 && deferred.maxFrameTimeMs < forward.maxFrameTimeMs)
            file << "Deferred |\n";
        else
            file << "Forward |\n";
        
        file << "\n## Performance Analysis\n\n";
        
        if (deferred.frameCount > 0)
        {
            double speedup = forward.totalTimeMs / deferred.totalTimeMs;
            file << "- **Speedup**: " << std::fixed << std::setprecision(2) << speedup << "x\n";
            file << "- **Forward Advantage**: " << std::fixed << std::setprecision(1) 
                 << ((forward.fps / deferred.fps - 1.0) * 100.0) << "% faster\n";
            file << "- **Deferred Advantage**: " << std::fixed << std::setprecision(1) 
                 << ((deferred.fps / forward.fps - 1.0) * 100.0) << "% faster\n";
        }
        else
        {
            file << "- Deferred renderer not available for comparison\n";
        }
        
        file << "\n## Notes\n";
        file << "- Forward rendering: Single-pass, calculates lighting per fragment\n";
        file << "- Deferred rendering: Two-pass (geometry + lighting), better for many lights\n";
        file << "- This scene has 5 lights, which may favor deferred rendering\n";
        
        file.close();
        printf("\nComparison report saved to: %s\n", outputPath.ToMultiByteString());
    }
    
    void GenerateCSVData(const NFLBenchmarkResult& forward, const NFLBenchmarkResult& deferred, const String& outputPath)
    {
        std::ofstream file(outputPath.ToMultiByteString());
        if (!file.is_open())
        {
            printf("ERROR: Could not open CSV file: %s\n", outputPath.ToMultiByteString());
            return;
        }
        
        file << "Renderer,TotalTimeMs,AvgFrameTimeMs,FPS,MinFrameTimeMs,MaxFrameTimeMs\n";
        file << "Forward," << std::fixed << std::setprecision(2) 
             << forward.totalTimeMs << "," << forward.avgFrameTimeMs << "," 
             << forward.fps << "," << forward.minFrameTimeMs << "," << forward.maxFrameTimeMs << "\n";
        
        if (deferred.frameCount > 0)
        {
            file << "Deferred," << deferred.totalTimeMs << "," << deferred.avgFrameTimeMs << "," 
                 << deferred.fps << "," << deferred.minFrameTimeMs << "," << deferred.maxFrameTimeMs << "\n";
        }
        
        file.close();
        printf("CSV data saved to: %s\n", outputPath.ToMultiByteString());
    }
    
    // Benchmark forward renderer with specific number of lights
    NFLBenchmarkResult BenchmarkForwardWithLights(int numLights, int maxFrames = 20)
    {
        printf("  Forward with %d lights (%d frames)...\n", numLights, maxFrames);
        fflush(stdout);
        
        FrameBuffer frameBuffer(width, height);
        IRasterRenderer* renderer = CreateTiledRenderer();
        renderer->SetFrameBuffer(&frameBuffer);
        
        RefPtr<NFLPlayScene> scene = new NFLPlayScene(viewSettings, stadiumModelPath, playData);
        
        ForwardLightingShader* shader = new ForwardLightingShader();
        shader->CameraPosition = Vec3(60.0f, 60.0f, 50.0f);
        shader->Shininess = 32.0f;
        shader->SpecularColor = Vec3(0.5f, 0.5f, 0.5f);
        SetupLightsWithCount(shader, numLights);
        scene->SetShader(shader);
        
        // Warmup
        scene->SetStep(playData.steps[0]);
        renderer->Clear(scene->ClearColor);
        scene->Draw(renderer);
        renderer->Finish();
        
        // Benchmark (use limited frames for speed)
        double minTime = 1e10;
        double maxTime = 0.0;
        double totalTime = 0.0;
        int frameCount = std::min(maxFrames, (int)playData.steps.size());
        
        for (int i = 0; i < frameCount; i++)
        {
            int step = playData.steps[i];
            scene->SetStep(step);
            
            auto counter = PerformanceCounter::Start();
            renderer->Clear(scene->ClearColor);
            scene->Draw(renderer);
            renderer->Finish();
            auto elapsed = PerformanceCounter::End(counter);
            double frameTime = PerformanceCounter::ToSeconds(elapsed) * 1000.0;
            
            totalTime += frameTime;
            if (frameTime < minTime) minTime = frameTime;
            if (frameTime > maxTime) maxTime = frameTime;
        }
        
        NFLBenchmarkResult result;
        result.rendererName = L"Forward";
        result.frameCount = frameCount;
        result.lightCount = numLights;
        result.totalTimeMs = totalTime;
        result.avgFrameTimeMs = totalTime / frameCount;
        result.fps = 1000.0 / result.avgFrameTimeMs;
        result.minFrameTimeMs = minTime;
        result.maxFrameTimeMs = maxTime;
        
        DestroyRenderer(renderer);
        return result;
    }
    
    // Benchmark deferred renderer with specific number of lights
    NFLBenchmarkResult BenchmarkDeferredWithLights(int numLights, int maxFrames = 20)
    {
        printf("  Deferred with %d lights (%d frames)...\n", numLights, maxFrames);
        fflush(stdout);
        
        FrameBuffer frameBuffer(width, height);
        IRasterRenderer* renderer = CreateDeferredTiledRenderer();
        renderer->SetFrameBuffer(&frameBuffer);
        
        RefPtr<NFLPlayScene> scene = new NFLPlayScene(viewSettings, stadiumModelPath, playData);
        
        ForwardLightingShader* shader = new ForwardLightingShader();
        shader->CameraPosition = Vec3(60.0f, 60.0f, 50.0f);
        shader->Shininess = 32.0f;
        shader->SpecularColor = Vec3(0.5f, 0.5f, 0.5f);
        SetupLightsWithCount(shader, numLights);
        scene->SetShader(shader);
        
        // Warmup
        scene->SetStep(playData.steps[0]);
        renderer->Clear(scene->ClearColor);
        scene->Draw(renderer);
        renderer->Finish();
        
        // Benchmark (use limited frames for speed)
        double minTime = 1e10;
        double maxTime = 0.0;
        double totalTime = 0.0;
        int frameCount = std::min(maxFrames, (int)playData.steps.size());
        
        for (int i = 0; i < frameCount; i++)
        {
            int step = playData.steps[i];
            scene->SetStep(step);
            
            auto counter = PerformanceCounter::Start();
            renderer->Clear(scene->ClearColor);
            scene->Draw(renderer);
            renderer->Finish();
            auto elapsed = PerformanceCounter::End(counter);
            double frameTime = PerformanceCounter::ToSeconds(elapsed) * 1000.0;
            
            totalTime += frameTime;
            if (frameTime < minTime) minTime = frameTime;
            if (frameTime > maxTime) maxTime = frameTime;
        }
        
        NFLBenchmarkResult result;
        result.rendererName = L"Deferred";
        result.frameCount = frameCount;
        result.lightCount = numLights;
        result.totalTimeMs = totalTime;
        result.avgFrameTimeMs = totalTime / frameCount;
        result.fps = 1000.0 / result.avgFrameTimeMs;
        result.minFrameTimeMs = minTime;
        result.maxFrameTimeMs = maxTime;
        
        DestroyRenderer(renderer);
        return result;
    }
    
    // Run comparison with varying light counts
    std::vector<LightScalingResult> RunLightScalingComparison(int framesPerTest = 20)
    {
        printf("\n========================================\n");
        printf("=== LIGHT SCALING COMPARISON ===\n");
        printf("========================================\n");
        printf("Testing: Forward vs Deferred with varying light counts\n");
        printf("Frames per test: %d\n\n", framesPerTest);
        
        std::vector<int> lightCounts = {1, 5, 10, 25, 50, 75, 100};
        std::vector<LightScalingResult> results;
        
        for (int numLights : lightCounts)
        {
            printf("\n--- Testing with %d lights ---\n", numLights);
            fflush(stdout);
            
            LightScalingResult result;
            result.lightCount = numLights;
            
            try {
                NFLBenchmarkResult fwd = BenchmarkForwardWithLights(numLights, framesPerTest);
                result.forwardFps = fwd.fps;
                result.forwardTimeMs = fwd.avgFrameTimeMs;
                printf("    Forward: %.2f FPS (%.2f ms/frame)\n", fwd.fps, fwd.avgFrameTimeMs);
            } catch (...) {
                printf("    Forward: FAILED\n");
                result.forwardFps = 0;
                result.forwardTimeMs = 0;
            }
            
            try {
                NFLBenchmarkResult def = BenchmarkDeferredWithLights(numLights, framesPerTest);
                result.deferredFps = def.fps;
                result.deferredTimeMs = def.avgFrameTimeMs;
                printf("    Deferred: %.2f FPS (%.2f ms/frame)\n", def.fps, def.avgFrameTimeMs);
            } catch (...) {
                printf("    Deferred: FAILED\n");
                result.deferredFps = 0;
                result.deferredTimeMs = 0;
            }
            
            // Calculate speedup (forward time / deferred time)
            // > 1.0 means deferred is faster
            if (result.deferredTimeMs > 0 && result.forwardTimeMs > 0)
            {
                result.speedup = result.forwardTimeMs / result.deferredTimeMs;
                result.winner = (result.speedup > 1.0) ? L"Deferred" : L"Forward";
                printf("    Winner: %s (%.2fx)\n", result.winner.ToMultiByteString(), result.speedup);
            }
            else
            {
                result.speedup = 0;
                result.winner = L"N/A";
            }
            
            results.push_back(result);
            fflush(stdout);
        }
        
        return results;
    }
    
    // Generate light scaling report
    void GenerateLightScalingReport(const std::vector<LightScalingResult>& results, const String& outputPath)
    {
        std::ofstream file(outputPath.ToMultiByteString());
        if (!file.is_open())
        {
            printf("ERROR: Could not open output file: %s\n", outputPath.ToMultiByteString());
            return;
        }
        
        file << "# Light Scaling Comparison Report\n";
        file << "# Forward vs Deferred Rendering with Varying Light Counts\n\n";
        
        file << "## Summary\n\n";
        file << "This test compares forward and deferred rendering performance as the number\n";
        file << "of lights increases. Forward rendering has O(fragments × lights) complexity,\n";
        file << "while deferred rendering has O(fragments + lights × affected_pixels) complexity.\n\n";
        
        file << "**Theoretical crossover point**: Deferred becomes faster when light count is high\n";
        file << "enough that the G-Buffer overhead is offset by reduced per-fragment light calculations.\n\n";
        
        // Find crossover point
        int crossoverLights = -1;
        for (const auto& r : results)
        {
            if (r.speedup > 1.0)
            {
                crossoverLights = r.lightCount;
                break;
            }
        }
        
        if (crossoverLights > 0)
        {
            file << "**Crossover Point**: ~" << crossoverLights << " lights\n";
            file << "(Deferred becomes faster at around this many lights)\n\n";
        }
        else
        {
            file << "**Crossover Point**: Not reached in this test (Forward always faster)\n\n";
        }
        
        file << "## Results\n\n";
        file << "| Lights | Forward FPS | Forward ms | Deferred FPS | Deferred ms | Winner | Speedup |\n";
        file << "|--------|-------------|------------|--------------|-------------|--------|----------|\n";
        
        for (const auto& r : results)
        {
            file << "| " << r.lightCount << " | ";
            file << std::fixed << std::setprecision(2) << r.forwardFps << " | ";
            file << r.forwardTimeMs << " | ";
            file << r.deferredFps << " | ";
            file << r.deferredTimeMs << " | ";
            file << r.winner.ToMultiByteString() << " | ";
            file << r.speedup << "x |\n";
        }
        
        file << "\n## Analysis\n\n";
        file << "### Performance Trends\n\n";
        
        if (results.size() >= 2)
        {
            double forwardSlowdown = results.back().forwardTimeMs / results.front().forwardTimeMs;
            double deferredSlowdown = results.back().deferredTimeMs / results.front().deferredTimeMs;
            
            file << "- **Forward slowdown** (1 → " << results.back().lightCount << " lights): ";
            file << std::fixed << std::setprecision(2) << forwardSlowdown << "x\n";
            file << "- **Deferred slowdown** (1 → " << results.back().lightCount << " lights): ";
            file << deferredSlowdown << "x\n\n";
            
            file << "### Observations\n\n";
            if (forwardSlowdown > deferredSlowdown)
            {
                file << "- Forward rendering scales poorly with light count (";
                file << forwardSlowdown << "x slowdown)\n";
                file << "- Deferred rendering handles many lights more efficiently (";
                file << deferredSlowdown << "x slowdown)\n";
            }
            else
            {
                file << "- For this scene, forward rendering remains competitive\n";
                file << "- Deferred rendering overhead may not be worth it for low light counts\n";
            }
        }
        
        file << "\n## Notes\n\n";
        file << "- Forward rendering: Single-pass, O(fragments × lights)\n";
        file << "- Deferred rendering: Multi-pass, O(fragments + lights × pixels_per_light)\n";
        file << "- Deferred has fixed overhead from G-Buffer generation\n";
        file << "- Crossover point depends on scene complexity, resolution, and light properties\n";
        
        file.close();
        printf("\nLight scaling report saved to: %s\n", outputPath.ToMultiByteString());
    }
    
    // Generate light scaling CSV
    void GenerateLightScalingCSV(const std::vector<LightScalingResult>& results, const String& outputPath)
    {
        std::ofstream file(outputPath.ToMultiByteString());
        if (!file.is_open())
        {
            printf("ERROR: Could not open CSV file: %s\n", outputPath.ToMultiByteString());
            return;
        }
        
        file << "LightCount,ForwardFPS,ForwardTimeMs,DeferredFPS,DeferredTimeMs,Winner,Speedup\n";
        
        for (const auto& r : results)
        {
            file << r.lightCount << ",";
            file << std::fixed << std::setprecision(2);
            file << r.forwardFps << ",";
            file << r.forwardTimeMs << ",";
            file << r.deferredFps << ",";
            file << r.deferredTimeMs << ",";
            file << r.winner.ToMultiByteString() << ",";
            file << r.speedup << "\n";
        }
        
        file.close();
        printf("Light scaling CSV saved to: %s\n", outputPath.ToMultiByteString());
    }
};

int main(int argc, char* argv[])
{
    printf("=== NFL Renderer Comparison Tool ===\n");
    
    if (argc < 4)
    {
        printf("Usage: %s <tracking_csv> <game_play> <stadium_model.obj> [output_dir] [width] [height] [--light-scaling]\n", argv[0]);
        printf("\nExamples:\n");
        printf("  Basic comparison (5 lights, full animation):\n");
        printf("    %s tracking.csv 58580_001136 stadium.obj output/ 1920 1080\n", argv[0]);
        printf("\n  Light scaling test (1-100 lights, 20 frames each):\n");
        printf("    %s tracking.csv 58580_001136 stadium.obj output/ 1920 1080 --light-scaling\n", argv[0]);
        return 1;
    }
    
    String trackingCsv = argv[1];
    String gamePlay = argv[2];
    String stadiumModel = argv[3];
    String outputDir = (argc > 4) ? String(argv[4]) : String(L"output");
    int width = (argc > 5) ? StringToInt(argv[5]) : 1920;
    int height = (argc > 6) ? StringToInt(argv[6]) : 1080;
    
    // Check for --light-scaling flag
    bool runLightScaling = false;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--light-scaling") == 0)
        {
            runLightScaling = true;
            break;
        }
    }
    
    printf("Configuration:\n");
    printf("  Tracking CSV: %s\n", trackingCsv.ToMultiByteString());
    printf("  Game Play: %s\n", gamePlay.ToMultiByteString());
    printf("  Stadium Model: %s\n", stadiumModel.ToMultiByteString());
    printf("  Output Dir: %s\n", outputDir.ToMultiByteString());
    printf("  Resolution: %dx%d\n", width, height);
    
    // Load play data
    printf("\nLoading play data...\n");
    PlayData playData;
    try {
        playData = TrackingDataLoader::GetPlay(trackingCsv, gamePlay);
        printf("Loaded %d frames, %d players\n", (int)playData.steps.size(), (int)playData.players.size());
    }
    catch (Exception& ex) {
        printf("ERROR: Could not load play data: %s\n", ex.Message.ToMultiByteString());
        return 1;
    }
    
    // Create comparison tool
    NFLRendererComparison comparison(width, height, stadiumModel, playData);
    
    if (runLightScaling)
    {
        // Run light scaling comparison
        printf("\n=== Running Light Scaling Comparison ===\n");
        printf("This test compares forward vs deferred rendering with 1-100 lights\n");
        printf("to find the crossover point where deferred becomes faster.\n");
        
        auto lightResults = comparison.RunLightScalingComparison(20); // 20 frames per test
        
        // Generate light scaling reports
        String lightReportPath = Path::Combine(outputDir, L"light_scaling_comparison.md");
        String lightCsvPath = Path::Combine(outputDir, L"light_scaling_comparison.csv");
        
        comparison.GenerateLightScalingReport(lightResults, lightReportPath);
        comparison.GenerateLightScalingCSV(lightResults, lightCsvPath);
        
        // Print summary
        printf("\n========================================\n");
        printf("=== LIGHT SCALING SUMMARY ===\n");
        printf("========================================\n");
        printf("\n| Lights | Forward FPS | Deferred FPS | Winner    |\n");
        printf("|--------|-------------|--------------|----------|\n");
        
        int crossover = -1;
        for (const auto& r : lightResults)
        {
            printf("| %6d | %11.2f | %12.2f | %-8s |\n", 
                   r.lightCount, r.forwardFps, r.deferredFps, 
                   r.winner.ToMultiByteString());
            
            if (crossover == -1 && r.speedup > 1.0)
            {
                crossover = r.lightCount;
            }
        }
        
        printf("\n");
        if (crossover > 0)
        {
            printf("CROSSOVER POINT: ~%d lights\n", crossover);
            printf("(Deferred becomes faster at around %d lights)\n", crossover);
        }
        else
        {
            printf("CROSSOVER POINT: Not reached\n");
            printf("(Forward remained faster for all tested light counts)\n");
        }
        
        printf("\nReports saved to:\n");
        printf("  %s\n", lightReportPath.ToMultiByteString());
        printf("  %s\n", lightCsvPath.ToMultiByteString());
    }
    else
    {
        // Run standard comparison (5 lights, full animation)
        
        // Benchmark forward renderer
        printf("\n=== Starting Forward Renderer Benchmark ===\n");
        fflush(stdout);
        NFLBenchmarkResult forwardResult;
        try {
            forwardResult = comparison.BenchmarkForward();
            printf("=== Forward Renderer Benchmark Complete ===\n");
            fflush(stdout);
        } catch (...) {
            printf("ERROR: Exception in forward benchmark. Continuing...\n");
            fflush(stdout);
            forwardResult.rendererName = L"Forward";
            forwardResult.frameCount = 0;
            forwardResult.lightCount = 5;
            forwardResult.totalTimeMs = 0;
            forwardResult.avgFrameTimeMs = 0;
            forwardResult.fps = 0;
            forwardResult.minFrameTimeMs = 0;
            forwardResult.maxFrameTimeMs = 0;
        }
        
        // Benchmark deferred renderer
        printf("\n=== Starting Deferred Renderer Benchmark ===\n");
        fflush(stdout);
        NFLBenchmarkResult deferredResult;
        try {
            deferredResult = comparison.BenchmarkDeferred();
            printf("=== Deferred Renderer Benchmark Complete ===\n");
            fflush(stdout);
        } catch (...) {
            printf("ERROR: Exception in deferred benchmark. Continuing...\n");
            fflush(stdout);
            deferredResult.rendererName = L"Deferred";
            deferredResult.frameCount = 0;
            deferredResult.lightCount = 5;
            deferredResult.totalTimeMs = 0;
            deferredResult.avgFrameTimeMs = 0;
            deferredResult.fps = 0;
            deferredResult.minFrameTimeMs = 0;
            deferredResult.maxFrameTimeMs = 0;
        }
        
        // Generate reports
        String reportPath = Path::Combine(outputDir, L"nfl_renderer_comparison.md");
        String csvPath = Path::Combine(outputDir, L"nfl_renderer_comparison.csv");
        
        comparison.GenerateComparisonReport(forwardResult, deferredResult, reportPath);
        comparison.GenerateCSVData(forwardResult, deferredResult, csvPath);
        
        printf("\n=== Comparison Complete ===\n");
        printf("Forward: %.2f FPS (%.2f ms/frame)\n", forwardResult.fps, forwardResult.avgFrameTimeMs);
        if (deferredResult.frameCount > 0)
        {
            printf("Deferred: %.2f FPS (%.2f ms/frame)\n", deferredResult.fps, deferredResult.avgFrameTimeMs);
            double speedup = forwardResult.totalTimeMs / deferredResult.totalTimeMs;
            printf("Speedup: %.2fx\n", speedup);
        }
        else
        {
            printf("Deferred: Not available\n");
        }
    }
    
    return 0;
}

