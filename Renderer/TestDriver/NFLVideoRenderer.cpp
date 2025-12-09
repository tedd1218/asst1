#include "CoreLib/Basic.h"
#include "IRasterRenderer.h"
#include "TestScene.h"
#include "ViewSettings.h"
#include "ForwardLightingShader.h"
#include "NFLTrackingData.h"
#include "NFLScene.h"
#include "ModelResource.h"
#include "CoreLib/PerformanceCounter.h"
#include "CoreLib/LibIO.h"
#include "CoreLib/VectorMath.h"
#include "CoreLib/Graphics/ObjModel.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

using namespace CoreLib::Basic;
using namespace CoreLib::Diagnostics;
using namespace CoreLib::Graphics;
using namespace CoreLib::IO;
using namespace RasterRenderer;
using namespace Testing;
using namespace VectorMath;
using namespace NFL;

// Simple player model implementation
// (Declaration is in NFLScene.h)
SimplePlayerModel::SimplePlayerModel(const Vec3& color)
{
        // Create a simple box model for players
        ObjModel obj;
        
        // Create a simple box for a player
        // Players are roughly 0.3-0.5 yards wide and about 2 yards (6 feet) tall
        float w = 0.2f; // Half width (0.4 yards = ~1.2 feet wide)
        float h = 2.0f;  // Height (2 yards = 6 feet tall)
        
        // 8 vertices of a box
        obj.Vertices.Add(Vec3(-w, -w, 0));  // 0: bottom front left
        obj.Vertices.Add(Vec3(w, -w, 0));   // 1: bottom front right
        obj.Vertices.Add(Vec3(w, w, 0));    // 2: bottom back right
        obj.Vertices.Add(Vec3(-w, w, 0));   // 3: bottom back left
        obj.Vertices.Add(Vec3(-w, -w, h));  // 4: top front left
        obj.Vertices.Add(Vec3(w, -w, h));   // 5: top front right
        obj.Vertices.Add(Vec3(w, w, h));    // 6: top back right
        obj.Vertices.Add(Vec3(-w, w, h));   // 7: top back left
        
        // Create 12 faces (2 per side)
        ObjFace face;
        face.NormalIds[0] = face.NormalIds[1] = face.NormalIds[2] = -1;
        face.TexCoordIds[0] = face.TexCoordIds[1] = face.TexCoordIds[2] = -1;
        face.MaterialId = 0;
        face.SmoothGroup = 0;
        
        // Bottom face
        face.VertexIds[0] = 0; face.VertexIds[1] = 1; face.VertexIds[2] = 2;
        obj.Faces.Add(face);
        face.VertexIds[0] = 0; face.VertexIds[1] = 2; face.VertexIds[2] = 3;
        obj.Faces.Add(face);
        
        // Top face
        face.VertexIds[0] = 4; face.VertexIds[1] = 6; face.VertexIds[2] = 5;
        obj.Faces.Add(face);
        face.VertexIds[0] = 4; face.VertexIds[1] = 7; face.VertexIds[2] = 6;
        obj.Faces.Add(face);
        
        // Front face
        face.VertexIds[0] = 0; face.VertexIds[1] = 5; face.VertexIds[2] = 1;
        obj.Faces.Add(face);
        face.VertexIds[0] = 0; face.VertexIds[1] = 4; face.VertexIds[2] = 5;
        obj.Faces.Add(face);
        
        // Back face
        face.VertexIds[0] = 2; face.VertexIds[1] = 7; face.VertexIds[2] = 3;
        obj.Faces.Add(face);
        face.VertexIds[0] = 2; face.VertexIds[1] = 6; face.VertexIds[2] = 7;
        obj.Faces.Add(face);
        
        // Left face
        face.VertexIds[0] = 0; face.VertexIds[1] = 3; face.VertexIds[2] = 7;
        obj.Faces.Add(face);
        face.VertexIds[0] = 0; face.VertexIds[1] = 7; face.VertexIds[2] = 4;
        obj.Faces.Add(face);
        
        // Right face
        face.VertexIds[0] = 1; face.VertexIds[1] = 6; face.VertexIds[2] = 2;
        obj.Faces.Add(face);
        face.VertexIds[0] = 1; face.VertexIds[1] = 5; face.VertexIds[2] = 6;
        obj.Faces.Add(face);
        
        // Create material with color
        RefPtr<ObjMaterial> mat = new ObjMaterial();
        mat->Diffuse = color;
        mat->Specular = Vec3(0.2f, 0.2f, 0.2f);
        mat->SpecularRate = 32.0f;
        obj.Materials.Add(mat);
        
        // Recompute normals
        RecomputeNormals(obj);
        
        // Convert to ModelResource
        model = ModelResource::FromObjModel(L"", obj);
    }
    
void SimplePlayerModel::Draw(RenderState& state, IRasterRenderer* renderer)
{
    model.Draw(state, renderer);
}

void SimplePlayerModel::SetShader(Shader* shader)
{
    model.SetShader(shader);
}

// Simple field/ground plane model (not used anymore, but kept for reference)
class FieldModel
{
private:
    ModelResource model;
    
public:
    FieldModel()
    {
        // Create a simple green field plane
        // NFL field is 120 yards long and 53.3 yards wide
        ObjModel obj;
        
        float fieldLength = 120.0f;  // 120 yards
        float fieldWidth = 53.3f;    // 53.3 yards
        float halfLength = fieldLength * 0.5f;
        float halfWidth = fieldWidth * 0.5f;
        
        // Create a quad for the field (centered at origin, on Z=0 plane)
        // 4 vertices forming a rectangle
        obj.Vertices.Add(Vec3(-halfLength, -halfWidth, 0.0f));  // Bottom-left
        obj.Vertices.Add(Vec3(halfLength, -halfWidth, 0.0f));   // Bottom-right
        obj.Vertices.Add(Vec3(halfLength, halfWidth, 0.0f));   // Top-right
        obj.Vertices.Add(Vec3(-halfLength, halfWidth, 0.0f));   // Top-left
        
        // Create 2 triangles to form the quad
        ObjFace face;
        face.NormalIds[0] = face.NormalIds[1] = face.NormalIds[2] = -1;
        face.TexCoordIds[0] = face.TexCoordIds[1] = face.TexCoordIds[2] = -1;
        face.MaterialId = 0;
        face.SmoothGroup = 0;
        
        // First triangle
        face.VertexIds[0] = 0; face.VertexIds[1] = 1; face.VertexIds[2] = 2;
        obj.Faces.Add(face);
        // Second triangle
        face.VertexIds[0] = 0; face.VertexIds[1] = 2; face.VertexIds[2] = 3;
        obj.Faces.Add(face);
        
        // Create material with green color (grass color)
        ObjMaterial* mat = new ObjMaterial();
        mat->Diffuse = Vec3(0.2f, 0.6f, 0.2f);  // Green grass color
        mat->Specular = Vec3(0.1f, 0.1f, 0.1f);
        mat->SpecularRate = 16.0f;
        obj.Materials.Add(mat);
        
        // Recompute normals
        RecomputeNormals(obj);
        
        // Convert to ModelResource
        model = ModelResource::FromObjModel(L"", obj);
    }
    
    void Draw(RenderState& state, IRasterRenderer* renderer)
    {
        model.Draw(state, renderer);
    }
    
    void SetShader(Shader* shader)
    {
        model.SetShader(shader);
    }
};

// NFLPlayScene implementation
// (Declaration is in NFLScene.h)
Vec3 NFLPlayScene::GetTeamColor(const String& team)
{
    if (team == L"home")
        return Vec3(0.0f, 0.0f, 1.0f); // Blue
    else
        return Vec3(1.0f, 0.0f, 0.0f); // Red
}

NFLPlayScene::NFLPlayScene(ViewSettings& viewSettings, const String& stadiumModelPath, const PlayData& playData)
    : TestScene(viewSettings), currentStep(0)
{
    // Set clear color to light blue (sky color) instead of black
    ClearColor = Vec4(0.5f, 0.7f, 1.0f, 1.0f); // Light blue sky
    
    // Load stadium model
    try
    {
        stadiumModel = ModelResource::FromObjModel(stadiumModelPath);
        printf("Loaded stadium model: %d triangles\n", stadiumModel.TriangleCount());
    }
    catch (Exception& ex)
    {
        printf("Warning: Could not load stadium model: %s\n", ex.Message.ToMultiByteString());
    }
    
    // Store player positions
    playerPositions = playData.players;
    steps = playData.steps;
    
    // Create player models (one per unique player)
    for (const auto& pair : playerPositions)
    {
        if (pair.second.size() > 0)
        {
            Vec3 color = GetTeamColor(pair.second[0].team);
            playerModels.push_back(SimplePlayerModel(color));
        }
    }
    
    printf("Created %d player models\n", (int)playerModels.size());
}
    
void NFLPlayScene::SetStep(int step)
{
    currentStep = step;
}

void NFLPlayScene::Draw(IRasterRenderer* renderer)
{
        
        Vec3 cameraPos(60.0f, 60.0f, 50.0f);   // Position above field (Z=50), offset in Y
        Vec3 target(60.0f, 26.65f, 0.0f);        // Center of field (Z=0) - looking down
        Vec3 worldUp(0.0f, 0.0f, 1.0f);         // Z is up (world up vector)
        
        Matrix4 viewMatrix;
        Matrix4::LookAt(viewMatrix, cameraPos, target, worldUp);
        
        // Flip the Y axis (up vector) to correct upside-down orientation
        viewMatrix.m[0][1] = -viewMatrix.m[0][1];  // Flip Y component of X axis (right)
        viewMatrix.m[1][1] = -viewMatrix.m[1][1];  // Flip Y component of Y axis (up)
        viewMatrix.m[2][1] = -viewMatrix.m[2][1];  // Flip Y component of Z axis (forward)
        viewMatrix.m[3][1] = -viewMatrix.m[3][1];  // Flip Y component of translation
        
        const float STADIUM_SCALE = 10.0f;
        
        Matrix4 stadiumScale, stadiumTranslation, stadiumTransform;
        Matrix4::CreateIdentityMatrix(stadiumScale);
        Matrix4::Scale(stadiumScale, STADIUM_SCALE, STADIUM_SCALE, STADIUM_SCALE);
        // Position stadium at field center (matching Python offset)
        Matrix4::Translation(stadiumTranslation, 60.0f, 26.65f, 0.0f);
        Matrix4::Multiply(stadiumTransform, stadiumTranslation, stadiumScale);
        
        // Combine view and model transforms
        Matrix4 stadiumModelView;
        Matrix4::Multiply(stadiumModelView, viewMatrix, stadiumTransform);
        State.ModelViewTransform = stadiumModelView;
        Matrix4::Multiply(State.ModelViewProjectionTransform, State.ProjectionTransform, stadiumModelView);
        // Normal transform should be inverse transpose of model-view for correct lighting
        stadiumModelView.Inverse(State.NormalTransform);
        State.NormalTransform.Transpose();
        
        // Disable backface culling temporarily to see if field geometry is being culled
        // Field geometry in OBJ might have normals pointing down instead of up
        bool oldBackfaceCulling = State.BackfaceCulling;
        State.BackfaceCulling = false;
        try {
            stadiumModel.Draw(State, renderer);
        } catch (...) {
            throw;
        }
        State.BackfaceCulling = oldBackfaceCulling;
        
        // Draw players at current step
        int playerIdx = 0;
        for (const auto& pair : playerPositions)
        {
            // Find position at current step
            const PlayerPosition* pos = nullptr;
            for (const auto& p : pair.second)
            {
                if (p.step == currentStep)
                {
                    pos = &p;
                    break;
                }
            }
            
            if (pos && playerIdx < (int)playerModels.size())
            {
                float worldX = pos->x;  // Absolute X coordinate (0-120 yards)
                float worldY = pos->y;  // Absolute Y coordinate (0-53.3 yards)
                float worldZ = 1.0f;    // Player center height (half of 2 yards tall) - bottom at Z=0
                
                // Create transform matrix for player
                // Scale player model to appropriate size (already scaled in model creation)
                Matrix4 playerScale, translation, rotation, playerModelTransform;
                Matrix4::CreateIdentityMatrix(playerScale);
                // Player model is already sized correctly (0.4 yards wide x 2 yards tall)
                // No additional scaling needed
                
                Matrix4::Translation(translation, worldX, worldY, worldZ);
                
                // Rotate based on orientation (around Z axis, which is up)
                float angleRad = pos->orientation * 3.14159f / 180.0f;
                Matrix4::RotationZ(rotation, angleRad);
                
                // Combine transforms: scale -> rotate -> translate
                Matrix4 temp;
                Matrix4::Multiply(temp, rotation, playerScale);
                Matrix4::Multiply(playerModelTransform, translation, temp);
                
                // Combine view and model transforms
                Matrix4 playerModelView;
                Matrix4::Multiply(playerModelView, viewMatrix, playerModelTransform);
                
                // Set transform
                State.ModelViewTransform = playerModelView;
                Matrix4::Multiply(State.ModelViewProjectionTransform, State.ProjectionTransform, playerModelView);
                // Normal transform should be inverse transpose of model-view for correct lighting
                playerModelView.Inverse(State.NormalTransform);
                State.NormalTransform.Transpose();
                
                // Draw player
                playerModels[playerIdx].Draw(State, renderer);
            }
            
            playerIdx++;
        }
    }
    
void NFLPlayScene::SetShader(Shader* shader)
{
    TestScene::SetShader(shader);
    stadiumModel.SetShader(shader);
    for (auto& player : playerModels)
    {
        player.SetShader(shader);
    }
}

// Main function - only compiled when NFL_VIDEO_RENDERER_MAIN is defined
#ifdef NFL_VIDEO_RENDERER_MAIN
int main(int argc, char* argv[])
{
    printf("=== NFL Video Renderer Starting ===\n");
    fflush(stdout);
    
    if (argc < 4)
    {
        printf("Usage: %s <tracking_csv> <game_play> <stadium_model.obj> [output_dir] [width] [height]\n", argv[0]);
        printf("Example: %s tracking.csv 58580_001136 stadium.obj output/ 1920 1080\n", argv[0]);
        return 1;
    }
    
    printf("Step 1: Parsing arguments...\n");
    String trackingCsv = argv[1];
    String gamePlay = argv[2];
    String stadiumModel = argv[3];
    String outputDir = (argc > 4) ? String(argv[4]) : String(L"output");
    int width = (argc > 5) ? StringToInt(argv[5]) : 1920;
    int height = (argc > 6) ? StringToInt(argv[6]) : 1080;
    
    printf("  Tracking CSV: %s\n", trackingCsv.ToMultiByteString());
    printf("  Game Play: %s\n", gamePlay.ToMultiByteString());
    printf("  Stadium Model: %s\n", stadiumModel.ToMultiByteString());
    printf("  Output Dir: %s\n", outputDir.ToMultiByteString());
    printf("  Resolution: %dx%d\n", width, height);
    
    // Load play data
    printf("\nStep 2: Loading play data...\n");
    PlayData playData;
    try {
        printf("  Calling TrackingDataLoader::GetPlay...\n");
        fflush(stdout);
        playData = TrackingDataLoader::GetPlay(trackingCsv, gamePlay);
        printf("  GetPlay returned, checking playData...\n");
        fflush(stdout);
        printf("  playData.steps.size() = %d\n", (int)playData.steps.size());
        fflush(stdout);
        printf("  playData.players.size() = %d\n", (int)playData.players.size());
        fflush(stdout);
    }
    catch (Exception& ex) {
        printf("Exception loading play data: %s\n", ex.Message.ToMultiByteString());
        return 1;
    }
    catch (std::exception& ex) {
        printf("std::exception loading play data: %s\n", ex.what());
        return 1;
    }
    catch (...) {
        printf("Unknown exception loading play data\n");
        return 1;
    }
    
    printf("  Play data loaded: %d steps, %d players\n", (int)playData.steps.size(), (int)playData.players.size());
    fflush(stdout);
    
    if (playData.steps.size() == 0)
    {
        printf("Error: Play not found or has no data\n");
        fflush(stdout);
        return 1;
    }
    
    printf("\nStep 3: Creating view settings...\n");
    fflush(stdout);
    // Create view settings
    ViewSettings viewSettings;
    printf("  ViewSettings object created\n");
    fflush(stdout);
    viewSettings.WindowWidth = width;
    viewSettings.WindowHeight = height;
    viewSettings.FovY = 60.0f;
    viewSettings.zNear = 0.1f;
    viewSettings.zFar = 500.0f;
    printf("  ViewSettings configured\n");
    fflush(stdout);
    
    printf("\nStep 4: Creating scene...\n");
    fflush(stdout);
    // Create scene
    NFLPlayScene* scene = nullptr;
    try {
        printf("  Creating NFLPlayScene with stadium: %s\n", stadiumModel.ToMultiByteString());
        fflush(stdout);
        scene = new NFLPlayScene(viewSettings, stadiumModel, playData);
        printf("  Scene created successfully\n");
        fflush(stdout);
        printf("  Scene pointer: %p\n", (void*)scene);
        fflush(stdout);
    }
    catch (Exception& ex) {
        printf("Exception creating scene: %s\n", ex.Message.ToMultiByteString());
        fflush(stdout);
        return 1;
    }
    catch (std::exception& ex) {
        printf("std::exception creating scene: %s\n", ex.what());
        fflush(stdout);
        return 1;
    }
    catch (...) {
        printf("Unknown exception creating scene\n");
        fflush(stdout);
        return 1;
    }
    
    printf("  After scene creation, before lighting setup\n");
    fflush(stdout);
    
    printf("\nStep 5: Setting up lighting...\n");
    fflush(stdout);
    // Set up lighting
    printf("  About to create ForwardLightingShader...\n");
    fflush(stdout);
    ForwardLightingShader* shader = new ForwardLightingShader();
    printf("  ForwardLightingShader created\n");
    fflush(stdout);
    
    printf("  Setting shader properties...\n");
    fflush(stdout);
    // Match camera position used in Draw() method
    shader->CameraPosition = Vec3(60.0f, 60.0f, 50.0f); // Camera looking at field
    printf("  CameraPosition set\n");
    fflush(stdout);
    shader->Shininess = 32.0f;
    printf("  Shininess set\n");
    fflush(stdout);
    shader->SpecularColor = Vec3(0.5f, 0.5f, 0.5f);
    printf("  SpecularColor set\n");
    fflush(stdout);
    
    // Add lights - match Python renderer setup
    printf("  Creating lights...\n");
    fflush(stdout);
    
    // Main directional light (sun) - pointing down at field
    ForwardLightingShader::Light sunLight;
    sunLight.LightType = ForwardLightingShader::Light::DIRECTIONAL;
    sunLight.Direction = Vec3(0.0f, 0.0f, -1.0f); // Pointing down in world space (Z is up)
    sunLight.Color = Vec3(1.0f, 1.0f, 0.95f);
    sunLight.Intensity = 4.0f; // Match Python intensity
    sunLight.Ambient = 0.4f;
    shader->Lights.Add(sunLight);
    printf("  Sun light added\n");
    fflush(stdout);
    
    // Add point lights at corners for better field illumination (matching Python)
    Vec3 corners[] = {
        Vec3(-10.0f, -10.0f, 50.0f),
        Vec3(130.0f, -10.0f, 50.0f),
        Vec3(130.0f, 63.0f, 50.0f),
        Vec3(-10.0f, 63.0f, 50.0f)
    };
    
    for (int i = 0; i < 4; i++)
    {
        ForwardLightingShader::Light pointLight;
        pointLight.LightType = ForwardLightingShader::Light::POINT;
        pointLight.Position = corners[i];
        pointLight.Color = Vec3(1.0f, 1.0f, 1.0f);
        pointLight.Intensity = 200.0f; // Match Python intensity
        pointLight.Ambient = 0.1f;
        pointLight.Decay = 100.0f; // Distance attenuation
        shader->Lights.Add(pointLight);
    }
    printf("  Point lights added\n");
    fflush(stdout);
    
    printf("  Setting shader on scene...\n");
    fflush(stdout);
    try {
        scene->SetShader(shader);
        printf("  Shader set on scene successfully\n");
        fflush(stdout);
    } catch (Exception& ex) {
        printf("  Exception setting shader: %s\n", ex.Message.ToMultiByteString());
        fflush(stdout);
        return 1;
    } catch (...) {
        printf("  Unknown exception setting shader\n");
        fflush(stdout);
        return 1;
    }
    
    printf("\nStep 6: Creating renderer...\n");
    fflush(stdout);
    // Create renderer
    printf("  Creating FrameBuffer (%dx%d)...\n", width, height);
    fflush(stdout);
    FrameBuffer frameBuffer(width, height);
    printf("  FrameBuffer created\n");
    fflush(stdout);
    printf("  Creating TiledRenderer...\n");
    fflush(stdout);
    IRasterRenderer* renderer = CreateTiledRenderer();
    printf("  TiledRenderer created\n");
    fflush(stdout);
    printf("  Setting FrameBuffer on renderer...\n");
    fflush(stdout);
    renderer->SetFrameBuffer(&frameBuffer);
    printf("  FrameBuffer set on renderer\n");
    fflush(stdout);
    
    printf("  Checking playData.steps.size()...\n");
    fflush(stdout);
    int numSteps = (int)playData.steps.size();
    printf("  numSteps = %d\n", numSteps);
    fflush(stdout);
    
    // Render each frame
    printf("\nStep 7: Rendering %d frames...\n", numSteps);
    fflush(stdout);
    printf("  Entering rendering loop...\n");
    fflush(stdout);
    
    for (size_t i = 0; i < playData.steps.size(); i++)
    {
        int step = playData.steps[i];
        scene->SetStep(step);
        
        // Render frame
        renderer->Clear(scene->ClearColor);
        scene->Draw(renderer);
        renderer->Finish();
        
        // Save frame - use zero-padded frame numbers for proper sorting
        char frameNumStr[32];
        snprintf(frameNumStr, sizeof(frameNumStr), "%05d", (int)i);
        String framePath = Path::Combine(outputDir, String(L"frame_") + String(frameNumStr) + L".bmp");
        try {
            frameBuffer.SaveColorBuffer(framePath);
        } catch (Exception& ex) {
            printf("Error saving frame %d: %s\n", (int)i, ex.Message.ToMultiByteString());
            fflush(stdout);
            // Continue with next frame
        }
        
        if ((i + 1) % 10 == 0 || i == 0)
        {
            printf("Rendered %d/%d frames (step %d)\n", (int)(i + 1), (int)playData.steps.size(), step);
            fflush(stdout);
        }
    }
    
    printf("\nRendering complete! Rendered %d frames total.\n", (int)playData.steps.size());
    fflush(stdout);
    printf("Frames saved to %s\n", outputDir.ToMultiByteString());
    fflush(stdout);
    
    // Count actual files created
    int fileCount = 0;
    for (int i = 0; i < (int)playData.steps.size(); i++)
    {
        char frameNumStr[32];
        snprintf(frameNumStr, sizeof(frameNumStr), "%05d", i);
        String framePath = Path::Combine(outputDir, String(L"frame_") + String(frameNumStr) + L".bmp");
        if (File::Exists(framePath))
        {
            fileCount++;
        }
    }
    printf("Actually created %d frame files\n", fileCount);
    fflush(stdout);
    printf("To create video: ffmpeg -r 10 -i %s/frame_%%05d.bmp -c:v libx264 -pix_fmt yuv420p output.mp4\n", 
           outputDir.ToMultiByteString());
    
    DestroyRenderer(renderer);
    return 0;
}
#endif // NFL_VIDEO_RENDERER_MAIN
