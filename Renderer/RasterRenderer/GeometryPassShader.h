#ifndef RASTER_RENDERER_GEOMETRY_PASS_SHADER_H
#define RASTER_RENDERER_GEOMETRY_PASS_SHADER_H

#include "Shader.h"
#include "RenderState.h"
#include "GBuffer.h"
#include "CoreLib/VectorMath.h"

namespace RasterRenderer
{
    using namespace VectorMath;

    // Geometry pass shader for deferred rendering
    // Writes world-space position, normal, albedo, and depth to G-Buffer
    class GeometryPassShader : public DefaultShader
    {
    public:
        GBuffer* gbuffer;  // G-Buffer to write to
        
        GeometryPassShader()
        {
            gbuffer = nullptr;
        }
        
        virtual void ShadeFragment(RenderState & state, float * output, __m128 * input, int id)
        {
            
            // Extract world position (input[7-9])
            CORE_LIB_ALIGN_16(float posX[4]);
            CORE_LIB_ALIGN_16(float posY[4]);
            CORE_LIB_ALIGN_16(float posZ[4]);
            
            _mm_store_ps(posX, input[7]);
            _mm_store_ps(posY, input[8]);
            _mm_store_ps(posZ, input[9]);
            
            // Extract normal (input[4-6]) and normalize
            __m128 nx2 = _mm_mul_ps(input[4], input[4]);
            __m128 ny2 = _mm_mul_ps(input[5], input[5]);
            __m128 nz2 = _mm_mul_ps(input[6], input[6]);
            __m128 normalLen = _mm_rsqrt_ps(_mm_add_ps(_mm_add_ps(nx2, ny2), nz2));
            __m128 normalX = _mm_mul_ps(input[4], normalLen);
            __m128 normalY = _mm_mul_ps(input[5], normalLen);
            __m128 normalZ = _mm_mul_ps(input[6], normalLen);
            
            CORE_LIB_ALIGN_16(float normX[4]);
            CORE_LIB_ALIGN_16(float normY[4]);
            CORE_LIB_ALIGN_16(float normZ[4]);
            
            _mm_store_ps(normX, normalX);
            _mm_store_ps(normY, normalY);
            _mm_store_ps(normZ, normalZ);
            
            // Extract depth (from clip space position w component)
            // For now, use z from input[9] as depth approximation
            CORE_LIB_ALIGN_16(float depth[4]);
            _mm_store_ps(depth, input[9]);
            
            // Extract albedo (default white, or from texture if available)
            CORE_LIB_ALIGN_16(float u[4]);
            CORE_LIB_ALIGN_16(float v[4]);
            _mm_store_ps(u, input[10]);
            _mm_store_ps(v, input[11]);
            
            // Store data in output buffer for later G-Buffer write
            // Layout: [posX0, posY0, posZ0, posX1, ...] [normX0, ...] [albedo0, ...] [depth0, ...]
            int offset = 0;
            for (int i = 0; i < 4; i++)
            {
                output[offset++] = posX[i];
                output[offset++] = posY[i];
                output[offset++] = posZ[i];
            }
            for (int i = 0; i < 4; i++)
            {
                output[offset++] = normX[i];
                output[offset++] = normY[i];
                output[offset++] = normZ[i];
            }
            
            // Albedo (default white, or sample texture)
            for (int i = 0; i < 4; i++)
            {
                Vec2 uv = Vec2(u[i], v[i]);
                Vec4 albedo = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
                
                // Sample texture if available
                TextureData * texture = *(TextureData **)((int*)state.ConstantBuffer + id*(4 + sizeof(TextureData*) / 4));
                if (texture)
                {
                    uv.x -= floor(uv.x);
                    uv.y -= floor(uv.y);
                    float dudx = fabs(u[1] - u[0]);
                    float dvdx = fabs(v[1] - v[0]);
                    float dudy = fabs(u[2] - u[0]);
                    float dvdy = fabs(v[2] - v[0]);
                    state.SampleTexture(&albedo, texture, 16, dudx, dvdx, dudy, dvdy, uv);
                }
                
                output[offset++] = albedo.x;
                output[offset++] = albedo.y;
                output[offset++] = albedo.z;
                output[offset++] = albedo.w;
            }
            
            // Depth
            for (int i = 0; i < 4; i++)
            {
                output[offset++] = depth[i];
            }
        }
    };
}

#endif

