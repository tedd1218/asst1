#ifndef RASTER_RENDERER_FORWARD_LIGHTING_SHADER_H
#define RASTER_RENDERER_FORWARD_LIGHTING_SHADER_H

#include "Shader.h"
#include "RenderState.h"
#include "CoreLib/VectorMath.h"
#include <immintrin.h>

namespace RasterRenderer
{
    using namespace VectorMath;

    // Forward lighting shader with Blinn-Phong specular highlights
    // Supports point lights, directional lights, and spot lights
    class ForwardLightingShader : public DefaultShader
    {
    public:
        // Light structure supporting multiple light types
        struct Light
        {
            Vec3 Position;          // For point/spot lights
            Vec3 Direction;         // For directional lights
            Vec3 Color;
            float Intensity;
            float Ambient;
            float Decay;            // For point lights (distance attenuation)
            enum Type { POINT, DIRECTIONAL, SPOT } LightType;
            
            // Spot light parameters
            float InnerConeAngle;   // Inner cone angle (cosine)
            float OuterConeAngle;   // Outer cone angle (cosine)
        };

        List<Light> Lights;
        Vec3 CameraPosition;        // World-space camera position for specular
        float Shininess;            // Specular shininess exponent (Blinn-Phong)
        Vec3 SpecularColor;         // Specular color (usually white or material color)

        ForwardLightingShader()
        {
            CameraPosition = Vec3(0.0f, 0.0f, 0.0f);
            Shininess = 32.0f;
            SpecularColor = Vec3(0.5f, 0.5f, 0.5f);
        }

        virtual void ShadeFragment(RenderState & state, float * output, __m128 * input, int id)
        {
            // input layout (from DefaultShader::ComputeVertex):
            // input[0-3]:   Clip space position (not used in fragment shader)
            // input[4-6]:   World-space normal (interpolated)
            // input[7-9]:   World-space position (view space in current code, but we treat as world)
            // input[10-11]: UV coordinates

            __m128 zero = _mm_setzero_ps();
            __m128 one = _mm_set1_ps(1.0f);
            
            // Accumulate lighting contributions
            __m128 sumDiffuseR = zero, sumDiffuseG = zero, sumDiffuseB = zero;
            __m128 sumSpecularR = zero, sumSpecularG = zero, sumSpecularB = zero;
            __m128 ambientR = zero, ambientG = zero, ambientB = zero;

            // Normalize normal (input[4-6])
            __m128 nx2 = _mm_mul_ps(input[4], input[4]);
            __m128 ny2 = _mm_mul_ps(input[5], input[5]);
            __m128 nz2 = _mm_mul_ps(input[6], input[6]);
            __m128 normalLen = _mm_rsqrt_ps(_mm_add_ps(_mm_add_ps(nx2, ny2), nz2));
            __m128 normalX = _mm_mul_ps(input[4], normalLen);
            __m128 normalY = _mm_mul_ps(input[5], normalLen);
            __m128 normalZ = _mm_mul_ps(input[6], normalLen);

            // Calculate view direction (from fragment to camera)
            __m128 viewX = _mm_sub_ps(_mm_set1_ps(CameraPosition.x), input[7]);
            __m128 viewY = _mm_sub_ps(_mm_set1_ps(CameraPosition.y), input[8]);
            __m128 viewZ = _mm_sub_ps(_mm_set1_ps(CameraPosition.z), input[9]);
            __m128 viewLen2 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(viewX, viewX), _mm_mul_ps(viewY, viewY)), _mm_mul_ps(viewZ, viewZ));
            __m128 viewLen = _mm_sqrt_ps(viewLen2);
            __m128 invViewLen = _mm_div_ps(one, viewLen);
            viewX = _mm_mul_ps(viewX, invViewLen);
            viewY = _mm_mul_ps(viewY, invViewLen);
            viewZ = _mm_mul_ps(viewZ, invViewLen);

            // Process each light
            for (auto &light : Lights)
            {
                __m128 lightDirX, lightDirY, lightDirZ;
                __m128 attenuation = one;
                __m128 spotFactor = one;

                if (light.LightType == Light::DIRECTIONAL)
                {
                    // Directional light: direction is constant
                    lightDirX = _mm_set1_ps(-light.Direction.x);
                    lightDirY = _mm_set1_ps(-light.Direction.y);
                    lightDirZ = _mm_set1_ps(-light.Direction.z);
                    attenuation = one; // No attenuation for directional lights
                }
                else
                {
                    // Point or spot light: calculate direction from position
                    __m128 lightX = _mm_sub_ps(_mm_set1_ps(light.Position.x), input[7]);
                    __m128 lightY = _mm_sub_ps(_mm_set1_ps(light.Position.y), input[8]);
                    __m128 lightZ = _mm_sub_ps(_mm_set1_ps(light.Position.z), input[9]);
                    
                    __m128 lightLen2 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(lightX, lightX), _mm_mul_ps(lightY, lightY)), _mm_mul_ps(lightZ, lightZ));
                    __m128 lightLen = _mm_sqrt_ps(lightLen2);
                    __m128 invLightLen = _mm_div_ps(one, lightLen);
                    
                    lightDirX = _mm_mul_ps(lightX, invLightLen);
                    lightDirY = _mm_mul_ps(lightY, invLightLen);
                    lightDirZ = _mm_mul_ps(lightZ, invLightLen);

                    // Distance attenuation for point lights
                    if (light.Decay > 0.01f)
                    {
                        attenuation = _mm_max_ps(zero, _mm_sub_ps(one, _mm_div_ps(lightLen, _mm_set_ps1(light.Decay))));
                    }

                    // Spot light cone
                    if (light.LightType == Light::SPOT)
                    {
                        __m128 spotDirX = _mm_set1_ps(-light.Direction.x);
                        __m128 spotDirY = _mm_set1_ps(-light.Direction.y);
                        __m128 spotDirZ = _mm_set1_ps(-light.Direction.z);
                        
                        __m128 spotDot = _mm_add_ps(_mm_add_ps(_mm_mul_ps(lightDirX, spotDirX), _mm_mul_ps(lightDirY, spotDirY)), _mm_mul_ps(lightDirZ, spotDirZ));
                        __m128 innerCone = _mm_set1_ps(light.InnerConeAngle);
                        __m128 outerCone = _mm_set1_ps(light.OuterConeAngle);
                        
                        // Smooth falloff from inner to outer cone
                        __m128 coneRange = _mm_sub_ps(innerCone, outerCone);
                        __m128 coneFactor = _mm_div_ps(_mm_sub_ps(spotDot, outerCone), coneRange);
                        spotFactor = _mm_max_ps(zero, _mm_min_ps(one, coneFactor));
                        attenuation = _mm_mul_ps(attenuation, spotFactor);
                    }
                }

                // Diffuse lighting: N·L
                __m128 NdotL = _mm_add_ps(_mm_add_ps(_mm_mul_ps(normalX, lightDirX), _mm_mul_ps(normalY, lightDirY)), _mm_mul_ps(normalZ, lightDirZ));
                NdotL = _mm_max_ps(zero, NdotL);

                // Apply attenuation
                __m128 effectiveLight = _mm_mul_ps(NdotL, attenuation);

                // Diffuse contribution
                __m128 diffuseContrib = _mm_mul_ps(effectiveLight, _mm_set_ps1(1.0f - light.Ambient));
                sumDiffuseR = _mm_add_ps(sumDiffuseR, _mm_mul_ps(_mm_set1_ps(light.Color.x), diffuseContrib));
                sumDiffuseG = _mm_add_ps(sumDiffuseG, _mm_mul_ps(_mm_set1_ps(light.Color.y), diffuseContrib));
                sumDiffuseB = _mm_add_ps(sumDiffuseB, _mm_mul_ps(_mm_set1_ps(light.Color.z), diffuseContrib));

                // Specular lighting: Blinn-Phong (N·H)^shininess
                // Half-vector H = normalize(L + V)
                __m128 halfX = _mm_add_ps(lightDirX, viewX);
                __m128 halfY = _mm_add_ps(lightDirY, viewY);
                __m128 halfZ = _mm_add_ps(lightDirZ, viewZ);
                
                __m128 halfLen2 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(halfX, halfX), _mm_mul_ps(halfY, halfY)), _mm_mul_ps(halfZ, halfZ));
                __m128 halfLen = _mm_rsqrt_ps(halfLen2);
                
                halfX = _mm_mul_ps(halfX, halfLen);
                halfY = _mm_mul_ps(halfY, halfLen);
                halfZ = _mm_mul_ps(halfZ, halfLen);

                // N·H
                __m128 NdotH = _mm_add_ps(_mm_add_ps(_mm_mul_ps(normalX, halfX), _mm_mul_ps(normalY, halfY)), _mm_mul_ps(normalZ, halfZ));
                NdotH = _mm_max_ps(zero, NdotH);

                // (N·H)^shininess using fast approximation
                // For integer shininess, we can use repeated multiplication
                // For now, use pow approximation
                __m128 shininessVec = _mm_set1_ps(Shininess);
                __m128 specularPower = NdotH;
                
                // Approximate power function (for performance)
                // For shininess = 32, we can use: x^32 ≈ (x^2)^16
                if (Shininess >= 16.0f)
                {
                    specularPower = _mm_mul_ps(specularPower, specularPower); // x^2
                    specularPower = _mm_mul_ps(specularPower, specularPower); // x^4
                    specularPower = _mm_mul_ps(specularPower, specularPower); // x^8
                    specularPower = _mm_mul_ps(specularPower, specularPower); // x^16
                    if (Shininess >= 32.0f)
                        specularPower = _mm_mul_ps(specularPower, specularPower); // x^32
                }
                else
                {
                    // For smaller shininess, use fewer multiplications
                    int powCount = (int)Shininess;
                    for (int i = 1; i < powCount; i *= 2)
                    {
                        specularPower = _mm_mul_ps(specularPower, specularPower);
                    }
                }

                // Specular contribution
                __m128 specularContrib = _mm_mul_ps(_mm_mul_ps(specularPower, effectiveLight), _mm_set_ps1(light.Intensity));
                sumSpecularR = _mm_add_ps(sumSpecularR, _mm_mul_ps(_mm_set1_ps(SpecularColor.x * light.Color.x), specularContrib));
                sumSpecularG = _mm_add_ps(sumSpecularG, _mm_mul_ps(_mm_set1_ps(SpecularColor.y * light.Color.y), specularContrib));
                sumSpecularB = _mm_add_ps(sumSpecularB, _mm_mul_ps(_mm_set1_ps(SpecularColor.z * light.Color.z), specularContrib));

                // Ambient contribution
                ambientR = _mm_add_ps(ambientR, _mm_set1_ps(light.Color.x * light.Ambient));
                ambientG = _mm_add_ps(ambientG, _mm_set1_ps(light.Color.y * light.Ambient));
                ambientB = _mm_add_ps(ambientB, _mm_set1_ps(light.Color.z * light.Ambient));
            }

            // Combine ambient, diffuse, and specular
            __m128 finalR = _mm_add_ps(_mm_add_ps(ambientR, sumDiffuseR), sumSpecularR);
            __m128 finalG = _mm_add_ps(_mm_add_ps(ambientG, sumDiffuseG), sumSpecularG);
            __m128 finalB = _mm_add_ps(_mm_add_ps(ambientB, sumDiffuseB), sumSpecularB);

            // Clamp to [0, 1]
            finalR = _mm_min_ps(_mm_max_ps(finalR, zero), one);
            finalG = _mm_min_ps(_mm_max_ps(finalG, zero), one);
            finalB = _mm_min_ps(_mm_max_ps(finalB, zero), one);

            // Store results (4 fragments in SIMD)
            CORE_LIB_ALIGN_16(float r[4]);
            CORE_LIB_ALIGN_16(float g[4]);
            CORE_LIB_ALIGN_16(float b[4]);
            CORE_LIB_ALIGN_16(float u[4]);
            CORE_LIB_ALIGN_16(float v[4]);

            _mm_store_ps(r, finalR);
            _mm_store_ps(g, finalG);
            _mm_store_ps(b, finalB);
            _mm_store_ps(u, input[10]);
            _mm_store_ps(v, input[11]);

            // Texture sampling (if needed)
            float dudx = fabs(u[1] - u[0]);
            float dudy = fabs(u[2] - u[0]);
            float dvdx = fabs(v[1] - v[0]);
            float dvdy = fabs(v[2] - v[0]);

            for (int i = 0; i < 4; i++)
            {
                Vec2 uv = Vec2(u[i], v[i]);
                
                // Sample texture if available
                Vec4 diffuseMap = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
                Vec4 diffRate = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
                
                if (state.ConstantBuffer)
                {
                    TextureData * texture = *(TextureData **)((int*)state.ConstantBuffer + id*(4 + sizeof(TextureData*) / 4));
                    if (texture)
                    {
                        uv.x -= floor(uv.x);
                        uv.y -= floor(uv.y);
                        state.SampleTexture(&diffuseMap, texture, 16, dudx, dvdx, dudy, dvdy, uv);
                    }
                    diffRate = *(Vec4*)((int*)state.ConstantBuffer + id*(4 + sizeof(TextureData*) / 4) + sizeof(TextureData*) / 4);
                }
                
                // Apply lighting to texture
                diffuseMap *= Vec4(r[i], g[i], b[i], 1.0f);
                diffuseMap *= diffRate;
                
                // Output RGBA for 4 fragments
                output[i] = diffuseMap.x;
                output[i + 4] = diffuseMap.y;
                output[i + 8] = diffuseMap.z;
                output[i + 12] = diffuseMap.w;
            }
        }
    };
}

#endif

