#ifndef RASTER_RENDERER_GBUFFER_H
#define RASTER_RENDERER_GBUFFER_H

#include "FrameBuffer.h"
#include "CoreLib/Basic.h"
#include "CoreLib/VectorMath.h"

namespace RasterRenderer
{
    using namespace CoreLib::Basic;
    using namespace VectorMath;

    // G-Buffer for deferred rendering
    // Stores geometric information per pixel for lighting calculations
    class GBuffer
    {
    private:
        int width, height;
        
        // G-Buffer buffers
        // Position: World-space position (RGB32F)
        List<Vec3> positionBuffer;
        
        // Normal: World-space normal (RGB16F - stored as float32 for simplicity)
        List<Vec3> normalBuffer;
        
        // Albedo: Base color (RGBA8 - stored as Vec4 for simplicity)
        List<Vec4> albedoBuffer;
        
        // Depth: Depth values (float32)
        List<float> depthBuffer;
        
    public:
        GBuffer()
        {
            width = height = 0;
        }
        
        GBuffer(int width, int height)
        {
            SetSize(width, height);
        }
        
        void SetSize(int width, int height)
        {
            this->width = width;
            this->height = height;
            
            int pixelCount = width * height;
            positionBuffer.SetSize(pixelCount);
            normalBuffer.SetSize(pixelCount);
            albedoBuffer.SetSize(pixelCount);
            depthBuffer.SetSize(pixelCount);
        }
        
        void Clear()
        {
            // Clear all buffers
            Vec3 zeroPos(0.0f, 0.0f, 0.0f);
            Vec3 zeroNormal(0.0f, 0.0f, 1.0f);  // Default normal pointing forward
            Vec4 zeroAlbedo(0.0f, 0.0f, 0.0f, 0.0f);
            
            for (int i = 0; i < positionBuffer.Count(); i++)
            {
                positionBuffer[i] = zeroPos;
                normalBuffer[i] = zeroNormal;
                albedoBuffer[i] = zeroAlbedo;
                depthBuffer[i] = 1.0f;  // Far plane
            }
        }
        
        // Write functions (for geometry pass)
        inline void SetPosition(int x, int y, const Vec3 & position)
        {
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                positionBuffer[y * width + x] = position;
            }
        }
        
        inline void SetNormal(int x, int y, const Vec3 & normal)
        {
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                normalBuffer[y * width + x] = normal;
            }
        }
        
        inline void SetAlbedo(int x, int y, const Vec4 & albedo)
        {
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                albedoBuffer[y * width + x] = albedo;
            }
        }
        
        inline void SetDepth(int x, int y, float depth)
        {
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                depthBuffer[y * width + x] = depth;
            }
        }
        
        // Read functions (for lighting pass)
        inline Vec3 GetPosition(int x, int y) const
        {
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                return positionBuffer[y * width + x];
            }
            return Vec3(0.0f, 0.0f, 0.0f);
        }
        
        inline Vec3 GetNormal(int x, int y) const
        {
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                return normalBuffer[y * width + x];
            }
            return Vec3(0.0f, 0.0f, 1.0f);
        }
        
        inline Vec4 GetAlbedo(int x, int y) const
        {
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                return albedoBuffer[y * width + x];
            }
            return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        
        inline float GetDepth(int x, int y) const
        {
            if (x >= 0 && x < width && y >= 0 && y < height)
            {
                return depthBuffer[y * width + x];
            }
            return 1.0f;
        }
        
        // Get buffer pointers (for bulk operations)
        inline Vec3 * GetPositionBuffer()
        {
            return positionBuffer.Buffer();
        }
        
        inline Vec3 * GetNormalBuffer()
        {
            return normalBuffer.Buffer();
        }
        
        inline Vec4 * GetAlbedoBuffer()
        {
            return albedoBuffer.Buffer();
        }
        
        inline float * GetDepthBuffer()
        {
            return depthBuffer.Buffer();
        }
        
        int GetWidth() const { return width; }
        int GetHeight() const { return height; }
        
        // Get memory usage (for debugging)
        size_t GetMemoryUsage() const
        {
            return positionBuffer.Count() * sizeof(Vec3) +
                   normalBuffer.Count() * sizeof(Vec3) +
                   albedoBuffer.Count() * sizeof(Vec4) +
                   depthBuffer.Count() * sizeof(float);
        }
    };
}

#endif

