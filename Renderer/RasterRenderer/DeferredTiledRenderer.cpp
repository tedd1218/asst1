#include "RendererImplBase.h"
#include "CommonTraceCollection.h"
#include "GBuffer.h"
#include "GeometryPassShader.h"
#include "LightingPassShader.h"
#include "ForwardLightingShader.h"
#include <algorithm>
#include <immintrin.h>

using namespace std;

namespace RasterRenderer
{
    class DeferredTiledRendererAlgorithm
    {
    private:
        // Tile configuration (same as forward renderer)
        static const int Log2TileSize = 5;
        static const int TileSize = 1 << Log2TileSize;

        // Render targets
        int gridWidth, gridHeight;
        FrameBuffer * frameBuffer;
        GBuffer * gbuffer;
        
        // Lighting data
        ForwardLightingShader::Light* lights;
        int lightCount;
        Vec3 cameraPosition;
        List<ForwardLightingShader::Light> lightsCopy; 
        
        struct TiledTriangle {
            ProjectedTriangle triangle;
            int threadId;
            int triangleIndex;
        };
        
        vector<vector<TiledTriangle>> tileBins;
        vector<vector<TiledTriangle>> localTileBins[Cores];
        
        // Shaders
        RefPtr<GeometryPassShader> geometryShader;
        RefPtr<LightingPassShader> lightingShader;
        
    public:
        inline void Init()
        {
            gbuffer = nullptr;
            lights = nullptr;
            lightCount = 0;
            geometryShader = new GeometryPassShader();
            lightingShader = new LightingPassShader();
        }
        
        inline void SetLights(ForwardLightingShader::Light* lightArray, int count, const Vec3& camPos)
        {
            lights = lightArray;
            lightCount = count;
            cameraPosition = camPos;
            
            // Update lighting shader
            lightingShader->lights = lights;
            lightingShader->lightCount = lightCount;
            lightingShader->cameraPosition = cameraPosition;
            lightingShader->shininess = 32.0f;
            lightingShader->specularColor = Vec3(0.5f, 0.5f, 0.5f);
        }

        inline void Clear(const Vec4 & clearColor, bool color, bool depth)
        {
            if (frameBuffer)
                frameBuffer->Clear(clearColor, color, depth);
            if (gbuffer)
                gbuffer->Clear();
            
            // Also clear tile bins to ensure clean state between frames
            for (auto& bin : tileBins) {
                bin.clear();
            }
        }

        inline void SetFrameBuffer(FrameBuffer * frameBuffer)
        {
            this->frameBuffer = frameBuffer;

            // Compute tile grid
            gridWidth = frameBuffer->GetWidth() >> Log2TileSize;
            gridHeight = frameBuffer->GetHeight() >> Log2TileSize;
            if (frameBuffer->GetWidth() & (TileSize - 1))
                gridWidth++;
            if (frameBuffer->GetHeight() & (TileSize - 1))
                gridHeight++;
            
            frameBuffer->Clear(Vec4(0,0,0,0), false, true);

            int numTiles = gridWidth * gridHeight;
            tileBins.assign(numTiles, vector<TiledTriangle>());
            
            for (int threadId = 0; threadId < Cores; threadId++) {
                localTileBins[threadId].assign(numTiles, vector<TiledTriangle>());
            }
            
            // Allocate G-Buffer
            if (gbuffer)
                delete gbuffer;
            gbuffer = new GBuffer(frameBuffer->GetWidth(), frameBuffer->GetHeight());
            gbuffer->Clear();
            
            // Set G-Buffer in shaders
            geometryShader->gbuffer = gbuffer;
            lightingShader->gbuffer = gbuffer;
        }

        inline void Finish()
        {
            // Cleanup if needed
        }

        inline void BinTriangles(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize, int threadId)
        {
            // Same binning as forward renderer
            auto & triangles = input.triangleBuffer[threadId];

            for (int i = 0; i < triangles.Count(); i++)
            {
                auto & tri = triangles[i];
                
                int bounds[4] = {
                    min(min(tri.X0, tri.X1), tri.X2) >> 4,
                    max(max(tri.X0, tri.X1), tri.X2) >> 4,
                    min(min(tri.Y0, tri.Y1), tri.Y2) >> 4,
                    max(max(tri.Y0, tri.Y1), tri.Y2) >> 4
                };
                
                bounds[0] = max(0, bounds[0]);
                bounds[1] = min(frameBuffer->GetWidth() - 1, bounds[1]);
                bounds[2] = max(0, bounds[2]);
                bounds[3] = min(frameBuffer->GetHeight() - 1, bounds[3]);
                
                int minX = bounds[0], maxX = bounds[1];
                int minY = bounds[2], maxY = bounds[3];
                
                int tileMinX = minX >> Log2TileSize;
                int tileMaxX = maxX >> Log2TileSize;
                int tileMinY = minY >> Log2TileSize;
                int tileMaxY = maxY >> Log2TileSize;
                
                for (int tileY = tileMinY; tileY <= tileMaxY; tileY++) {
                    for (int tileX = tileMinX; tileX <= tileMaxX; tileX++) {
                        int tileId = tileY * gridWidth + tileX;
                        if (tileId >= 0 && tileId < (int)localTileBins[threadId].size()) {
                            TiledTriangle tiledTri;
                            tiledTri.triangle = tri;
                            tiledTri.threadId = threadId;
                            tiledTri.triangleIndex = i;
                            localTileBins[threadId][tileId].push_back(tiledTri);
                        }
                    }
                }
            }
        }

        // Geometry Pass: Render triangles to G-Buffer
        inline void ProcessBinGeometryPass(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize, int tileId)
        {           
            if (tileId >= (int)tileBins.size())
            {
                printf("            ERROR: tileId %d >= tileBins.size() %zu\n", tileId, tileBins.size());
                fflush(stdout);
                return;
            }

            if (!gbuffer || !frameBuffer)
                return;
            
            int tileX = tileId % gridWidth;
            int tileY = tileId / gridWidth;
            int tilePixelX = tileX * TileSize;
            int tilePixelY = tileY * TileSize;
            int tilePixelW = min(TileSize, frameBuffer->GetWidth() - tilePixelX);
            int tilePixelH = min(TileSize, frameBuffer->GetHeight() - tilePixelY);
            
            static __m128i xOffset = _mm_set_epi32(24, 8, 24, 8);
            static __m128i yOffset = _mm_set_epi32(24, 24, 8, 8);
            
            // Store original shader
            Shader* originalShader = state.Shader;
            if (!geometryShader.Ptr())
                return;
            state.Shader = geometryShader.Ptr();
            
            // Validate tileId before accessing tileBins
            if (tileId < 0 || tileId >= (int)tileBins.size())
                return;
            
            for (const auto& tiledTri : tileBins[tileId]) {
                ProjectedTriangle tri = tiledTri.triangle;
                
                TriangleSIMD triSIMD;
                triSIMD.Load(tri);
                
                RasterizeTriangle(tilePixelX, tilePixelY, tilePixelW, tilePixelH, tri, triSIMD, 
                    [&](int qfx, int qfy, bool trivialAccept) {

                        __m128i coordX_center, coordY_center;
                        coordX_center = _mm_add_epi32(_mm_set1_epi32(qfx << 4), xOffset);
                        coordY_center = _mm_add_epi32(_mm_set1_epi32(qfy << 4), yOffset);
                        
                        int coverageMask = trivialAccept ? 0xFFFF : triSIMD.TestQuadFragment(coordX_center, coordY_center);
                        
                        auto zValues = triSIMD.GetZ(coordX_center, coordY_center);
                        CORE_LIB_ALIGN_16(float zStore[4]);
                        _mm_store_ps(zStore, zValues);
                        
                        FragmentCoverageMask visibility;

                        // Bounds check before accessing G-Buffer
                        int gbufferWidth = gbuffer->GetWidth();
                        int gbufferHeight = gbuffer->GetHeight();
                        
                        // Clamp coordinates to valid range
                        int px0 = min(max(qfx, 0), gbufferWidth - 1);
                        int py0 = min(max(qfy, 0), gbufferHeight - 1);
                        int px1 = min(max(qfx + 1, 0), gbufferWidth - 1);
                        int py1 = min(max(qfy + 1, 0), gbufferHeight - 1);
                        
                        // Use G-Buffer depth for depth testing
                        float gbufferDepth[4] = {
                            gbuffer->GetDepth(px1, py1),
                            gbuffer->GetDepth(px0, py1),
                            gbuffer->GetDepth(px1, py0),
                            gbuffer->GetDepth(px0, py0)
                        };
                        
                        __m128 currentZ = _mm_set_ps(gbufferDepth[0], gbufferDepth[1], gbufferDepth[2], gbufferDepth[3]);
                        __m128 depthMask = _mm_cmplt_ps(zValues, currentZ);
                        int depthMaskInt = _mm_movemask_ps(depthMask);
                        
                        // Pixel coordinates for 4 fragments in quad (clamped to bounds)
                        int pixelX[4] = { px0, px1, px0, px1 };
                        int pixelY[4] = { py0, py0, py1, py1 };
                        
                        if ((coverageMask & 0x0008) && (depthMaskInt & 0x1)) {
                            visibility.SetBit(0);
                            gbuffer->SetDepth(pixelX[0], pixelY[0], zStore[0]);
                        }
                        if ((coverageMask & 0x0080) && (depthMaskInt & 0x2)) {
                            visibility.SetBit(1);
                            gbuffer->SetDepth(pixelX[1], pixelY[1], zStore[1]);
                        }
                        if ((coverageMask & 0x0800) && (depthMaskInt & 0x4)) {
                            visibility.SetBit(2);
                            gbuffer->SetDepth(pixelX[2], pixelY[2], zStore[2]);
                        }
                        if ((coverageMask & 0x8000) && (depthMaskInt & 0x8)) {
                            visibility.SetBit(3);
                            gbuffer->SetDepth(pixelX[3], pixelY[3], zStore[3]);
                        }

                        if (visibility.Any()) {
                            __m128 gamma, beta, alpha;
                            triSIMD.GetCoordinates(gamma, alpha, beta, coordX_center, coordY_center);

                            // Interpolate vertex attributes using the same method as ShadeFragment
                            // Get triangle vertex indices from index buffer
                            int* indexBuffer = input.indexOutputBuffer[tiledTri.threadId].Buffer();
                            float* vertexBuffer = input.vertexOutputBuffer[tiledTri.threadId].Buffer();
                            
                            // Find triangle indices - need to track this through the pipeline
                            // For now, use a workaround: interpolate directly using barycentrics
                            // The vertex output buffer is indexed by vertex ID, not triangle ID
                            
                            // Extract barycentric coordinates for 4 fragments
                            CORE_LIB_ALIGN_16(float alphaVals[4]);
                            CORE_LIB_ALIGN_16(float betaVals[4]);
                            CORE_LIB_ALIGN_16(float gammaVals[4]);
                            _mm_store_ps(alphaVals, alpha);
                            _mm_store_ps(betaVals, beta);
                            _mm_store_ps(gammaVals, gamma);
                            
                            // Use ShadeFragment helper to get interpolated vertex output
                            // This gives us the interpolated __m128 array that shaders receive
                            __m128 interpolated[MaxVertexOutputSize];
                            
                            // Interpolate for each fragment and write to G-Buffer
                            for (int fragIdx = 0; fragIdx < 4; fragIdx++) {
                                if (visibility.GetBit(fragIdx)) {
                                    int px = pixelX[fragIdx];
                                    int py = pixelY[fragIdx];
                                    
                                    // Create single-fragment barycentrics
                                    __m128 fragAlpha = _mm_set1_ps(alphaVals[fragIdx]);
                                    __m128 fragBeta = _mm_set1_ps(betaVals[fragIdx]);
                                    __m128 fragGamma = _mm_set1_ps(gammaVals[fragIdx]);
                                    
                                    // Interpolate vertex output (same as ShadeFragment does internally)
                                    int triId = tri.Id;
                                    
                                    // Bounds check for safety
                                    int indexBufferSize = input.indexOutputBuffer[tiledTri.threadId].Count();
                                    int vertexBufferSize = input.vertexOutputBuffer[tiledTri.threadId].Count();
                                    int neededIdx = triId * 3 + 2;
                                    
                                    if (neededIdx >= indexBufferSize) {
                                        continue;
                                    }
                                    
                                    InterpolateVertexOutput(interpolated, state, fragBeta, fragGamma, fragAlpha,
                                        triId, vertexBuffer, vertexOutputSize, indexBuffer);
                                    
                                    // Extract world position (offset 7-9) and normal (offset 4-6)
                                    CORE_LIB_ALIGN_16(float posX[4]);
                                    CORE_LIB_ALIGN_16(float posY[4]);
                                    CORE_LIB_ALIGN_16(float posZ[4]);
                                    CORE_LIB_ALIGN_16(float normX[4]);
                                    CORE_LIB_ALIGN_16(float normY[4]);
                                    CORE_LIB_ALIGN_16(float normZ[4]);
                                    
                                    _mm_store_ps(posX, interpolated[7]);
                                    _mm_store_ps(posY, interpolated[8]);
                                    _mm_store_ps(posZ, interpolated[9]);
                                    _mm_store_ps(normX, interpolated[4]);
                                    _mm_store_ps(normY, interpolated[5]);
                                    _mm_store_ps(normZ, interpolated[6]);
                                    
                                    Vec3 worldPos(posX[0], posY[0], posZ[0]);
                                    Vec3 normal(normX[0], normY[0], normZ[0]);
                                    
                                    // Normalize normal
                                    float normalLen = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                                    if (normalLen > 0.001f) {
                                        normal.x /= normalLen;
                                        normal.y /= normalLen;
                                        normal.z /= normalLen;
                                    }
                                    
                                    // Default albedo (white) - could sample texture here
                                    Vec4 albedo(1.0f, 1.0f, 1.0f, 1.0f);
                                    
                                    // Write to G-Buffer
                                    gbuffer->SetPosition(px, py, worldPos);
                                    gbuffer->SetNormal(px, py, normal);
                                    gbuffer->SetAlbedo(px, py, albedo);
                                }
                            }
                        }
                    });
            }
            
            // Restore original shader
            state.Shader = originalShader;
        }

        // Lighting Pass: Read G-Buffer and calculate lighting
        inline void ProcessBinLightingPass(RenderState & state, int tileId)
        {
            if (tileId >= gridWidth * gridHeight) return;
            
            // Safety check: ensure G-Buffer is initialized
            if (!gbuffer)
            {
                printf("ERROR: G-Buffer is null in ProcessBinLightingPass!\n");
                return;
            }
            
            int tileX = tileId % gridWidth;
            int tileY = tileId / gridWidth;
            int tilePixelX = tileX * TileSize;
            int tilePixelY = tileY * TileSize;
            int tilePixelW = min(TileSize, frameBuffer->GetWidth() - tilePixelX);
            int tilePixelH = min(TileSize, frameBuffer->GetHeight() - tilePixelY);
            
            // Store original shader
            Shader* originalShader = state.Shader;
            state.Shader = lightingShader.Ptr();
            
            // Process each pixel in the tile
            for (int py = tilePixelY; py < tilePixelY + tilePixelH && py < frameBuffer->GetHeight(); py++) {
                for (int px = tilePixelX; px < tilePixelX + tilePixelW && px < frameBuffer->GetWidth(); px++) {
                    
                    // Read from G-Buffer
                    Vec3 worldPos = gbuffer->GetPosition(px, py);
                    Vec3 normal = gbuffer->GetNormal(px, py);
                    Vec4 albedo = gbuffer->GetAlbedo(px, py);
                    float depth = gbuffer->GetDepth(px, py);
                    
                    // Skip if no geometry (depth at far plane)
                    if (depth >= 0.99f)
                        continue;
                    
                    // Normalize normal
                    float normalLen = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                    if (normalLen > 0.001f) {
                        normal.x /= normalLen;
                        normal.y /= normalLen;
                        normal.z /= normalLen;
                    }
                    
                    // Calculate lighting (simplified - process one pixel at a time)
                    Vec3 color(0.1f, 0.1f, 0.1f); // Ambient
                    
                    // View direction
                    Vec3 viewDir = cameraPosition - worldPos;
                    float viewLen = sqrtf(viewDir.x * viewDir.x + viewDir.y * viewDir.y + viewDir.z * viewDir.z);
                    if (viewLen > 0.001f) {
                        viewDir.x /= viewLen;
                        viewDir.y /= viewLen;
                        viewDir.z /= viewLen;
                    }
                    
                    // Process each light
                    for (int lightIdx = 0; lightIdx < lightCount; lightIdx++) {
                        const ForwardLightingShader::Light& light = lights[lightIdx];
                        
                        Vec3 lightDir;
                        float attenuation = 1.0f;
                        
                        if (light.LightType == ForwardLightingShader::Light::DIRECTIONAL) {
                            lightDir = Vec3(-light.Direction.x, -light.Direction.y, -light.Direction.z);
                            attenuation = 1.0f;
                        } else {
                            Vec3 lightVec;
                            lightVec.x = light.Position.x - worldPos.x;
                            lightVec.y = light.Position.y - worldPos.y;
                            lightVec.z = light.Position.z - worldPos.z;
                            float lightLen = sqrtf(lightVec.x * lightVec.x + lightVec.y * lightVec.y + lightVec.z * lightVec.z);
                            if (lightLen > 0.001f) {
                                lightDir.x = lightVec.x / lightLen;
                                lightDir.y = lightVec.y / lightLen;
                                lightDir.z = lightVec.z / lightLen;
                                
                                if (light.Decay > 0.01f) {
                                    attenuation = fmaxf(0.0f, 1.0f - lightLen / light.Decay);
                                }
                                
                                if (light.LightType == ForwardLightingShader::Light::SPOT) {
                                    Vec3 spotDir(-light.Direction.x, -light.Direction.y, -light.Direction.z);
                                    float spotDot = lightDir.x * spotDir.x + lightDir.y * spotDir.y + lightDir.z * spotDir.z;
                                    if (spotDot < light.OuterConeAngle) {
                                        attenuation = 0.0f;
                                    } else if (spotDot < light.InnerConeAngle) {
                                        float coneFactor = (spotDot - light.OuterConeAngle) / (light.InnerConeAngle - light.OuterConeAngle);
                                        attenuation *= coneFactor;
                                    }
                                }
                            } else {
                                attenuation = 0.0f;
                            }
                        }
                    
                    if (attenuation > 0.001f) {
                        // Diffuse: N·L
                        float NdotL = normal.x * lightDir.x + normal.y * lightDir.y + normal.z * lightDir.z;
                        NdotL = fmaxf(0.0f, NdotL);
                        
                        if (NdotL > 0.0f) {
                            float diffuseContrib = NdotL * attenuation * (1.0f - light.Ambient);
                            color.x += albedo.x * light.Color.x * light.Intensity * diffuseContrib;
                            color.y += albedo.y * light.Color.y * light.Intensity * diffuseContrib;
                            color.z += albedo.z * light.Color.z * light.Intensity * diffuseContrib;
                            
                            // Specular: Blinn-Phong
                            Vec3 halfDir;
                            halfDir.x = lightDir.x + viewDir.x;
                            halfDir.y = lightDir.y + viewDir.y;
                            halfDir.z = lightDir.z + viewDir.z;
                            float halfLen = sqrtf(halfDir.x * halfDir.x + halfDir.y * halfDir.y + halfDir.z * halfDir.z);
                            if (halfLen > 0.001f) {
                                halfDir.x /= halfLen;
                                halfDir.y /= halfLen;
                                halfDir.z /= halfLen;
                                
                                float NdotH = normal.x * halfDir.x + normal.y * halfDir.y + normal.z * halfDir.z;
                                NdotH = fmaxf(0.0f, NdotH);
                                
                                // Approximate power
                                float specularPower = NdotH;
                                for (int i = 1; i < 32; i *= 2) {
                                    specularPower *= specularPower;
                                }
                                
                                float specularContrib = specularPower * NdotL * attenuation * light.Intensity;
                                color.x += 0.5f * light.Color.x * specularContrib;
                                color.y += 0.5f * light.Color.y * specularContrib;
                                color.z += 0.5f * light.Color.z * specularContrib;
                            }
                        }
                        
                        // Ambient
                        color.x += light.Color.x * light.Ambient;
                        color.y += light.Color.y * light.Ambient;
                        color.z += light.Color.z * light.Ambient;
                    }
                }
                    
                    // Clamp and write to framebuffer
                    color.x = fminf(1.0f, fmaxf(0.0f, color.x));
                    color.y = fminf(1.0f, fmaxf(0.0f, color.y));
                    color.z = fminf(1.0f, fmaxf(0.0f, color.z));
                    
                    frameBuffer->SetPixel(px, py, 0, Vec4(color.x, color.y, color.z, albedo.w));
                }
            }
            
            // Restore original shader
            state.Shader = originalShader;
        }

        inline void RenderProjectedBatch(RenderState & state, ProjectedTriangleInput & input, int vertexOutputSize)
        {
            // Safety check: ensure G-Buffer is initialized
            if (!gbuffer)
                return;
            
            // Get lights from RenderState if available
            ForwardLightingShader* forwardShader = dynamic_cast<ForwardLightingShader*>(state.Shader);
            
            if (forwardShader && forwardShader->Lights.Count() > 0)
            {
                // Copy lights from forward shader to avoid dangling pointer
                lightsCopy.Clear();
                for (int i = 0; i < forwardShader->Lights.Count(); i++)
                {
                    lightsCopy.Add(forwardShader->Lights[i]);
                }
                lights = lightsCopy.Buffer();
                lightCount = lightsCopy.Count();
                cameraPosition = forwardShader->CameraPosition;
                
                // Update lighting shader
                if (!lightingShader.Ptr())
                    return;
                lightingShader->lights = lights;
                lightingShader->lightCount = lightCount;
                lightingShader->cameraPosition = cameraPosition;
                lightingShader->shininess = forwardShader->Shininess;
                lightingShader->specularColor = forwardShader->SpecularColor;
            }
            else
            {
                // No lights - skip lighting pass
                lights = nullptr;
                lightCount = 0;
            }
            
            // Clear bins
            for (auto& bin : tileBins) {
                bin.clear();
            }
            
            for (int threadId = 0; threadId < Cores; threadId++) {
                for (auto& bin : localTileBins[threadId]) {
                    bin.clear();
                }
            }
            
            // Pass 1: Bin triangles
            Parallel::For(0, Cores, 1, [&](int threadId)
            {
                BinTriangles(state, input, vertexOutputSize, threadId);
            });

            // Merge local bins into global bins
            for (int threadId = 0; threadId < Cores; threadId++) {
                for (int tileId = 0; tileId < (int)tileBins.size(); tileId++) {
                    if (tileId < (int)localTileBins[threadId].size()) {
                        tileBins[tileId].insert(tileBins[tileId].end(), 
                                              localTileBins[threadId][tileId].begin(), 
                                              localTileBins[threadId][tileId].end());
                    }
                }
            }

            // Pass 2: Geometry Pass - Render to G-Buffer
            Parallel::For(0, gridWidth*gridHeight, 1, [&](int tileId)
            {
                ProcessBinGeometryPass(state, input, vertexOutputSize, tileId);
            });

            // Pass 3: Lighting Pass - Read G-Buffer and calculate lighting
            // Only do lighting pass if we have lights
            if (lightCount > 0 && lights != nullptr)
            {
                Parallel::For(0, gridWidth*gridHeight, 1, [&](int tileId)
                {
                    ProcessBinLightingPass(state, tileId);
                });
            }
        }
    };

    IRasterRenderer * CreateDeferredTiledRenderer()
    {
        return new RendererImplBase<DeferredTiledRendererAlgorithm>();
    }
    
    // Helper function to set lights on deferred renderer
    void SetDeferredRendererLights(IRasterRenderer* renderer, ForwardLightingShader::Light* lights, int count, const Vec3& cameraPos)
    {
        RendererImplBase<DeferredTiledRendererAlgorithm>* deferredRenderer = 
            static_cast<RendererImplBase<DeferredTiledRendererAlgorithm>*>(renderer);
        
        if (deferredRenderer)
        {
        }
    }
}

