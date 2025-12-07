#ifndef RASTER_RENDERER_LIGHTING_PASS_SHADER_H
#define RASTER_RENDERER_LIGHTING_PASS_SHADER_H

#include "Shader.h"
#include "RenderState.h"
#include "GBuffer.h"
#include "ForwardLightingShader.h"
#include "CoreLib/VectorMath.h"
#include <immintrin.h>

namespace RasterRenderer
{
    using namespace VectorMath;

    // Lighting pass shader for deferred rendering
    // Reads from G-Buffer and calculates lighting in screen-space
    class LightingPassShader : public DefaultShader
    {
    public:
        GBuffer* gbuffer;  // G-Buffer to read from
        ForwardLightingShader::Light* lights;  // Array of lights
        int lightCount;
        Vec3 cameraPosition;
        float shininess;
        Vec3 specularColor;
        
        LightingPassShader()
        {
            gbuffer = nullptr;
            lights = nullptr;
            lightCount = 0;
            cameraPosition = Vec3(0.0f, 0.0f, 0.0f);
            shininess = 32.0f;
            specularColor = Vec3(0.5f, 0.5f, 0.5f);
        }
        
        virtual void ShadeFragment(RenderState & state, float * output, __m128 * input, int id)
        {
            // This shader reads from G-Buffer and calculates lighting
            
            // Process 4 fragments (quad)
            CORE_LIB_ALIGN_16(float finalR[4]);
            CORE_LIB_ALIGN_16(float finalG[4]);
            CORE_LIB_ALIGN_16(float finalB[4]);
            CORE_LIB_ALIGN_16(float finalA[4]);
            
            __m128 zero = _mm_setzero_ps();
            __m128 one = _mm_set1_ps(1.0f);
            
            // Process 4 fragments
            for (int fragIdx = 0; fragIdx < 4; fragIdx++)
            {
                int pixelX = 0;  
                int pixelY = 0;
                
                // Read from G-Buffer
                Vec3 worldPos = gbuffer->GetPosition(pixelX, pixelY);
                Vec3 normal = gbuffer->GetNormal(pixelX, pixelY);
                Vec4 albedo = gbuffer->GetAlbedo(pixelX, pixelY);
                
                // Normalize normal
                float normalLen = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (normalLen > 0.001f)
                {
                    normal.x /= normalLen;
                    normal.y /= normalLen;
                    normal.z /= normalLen;
                }
                
                // Start with ambient
                Vec3 color;
                color.x = 0.1f * albedo.x;
                color.y = 0.1f * albedo.y;
                color.z = 0.1f * albedo.z;
                
                // View direction
                Vec3 viewDir = cameraPosition - worldPos;
                float viewLen = sqrtf(viewDir.x * viewDir.x + viewDir.y * viewDir.y + viewDir.z * viewDir.z);
                if (viewLen > 0.001f)
                {
                    viewDir.x /= viewLen;
                    viewDir.y /= viewLen;
                    viewDir.z /= viewLen;
                }
                
                // Process each light
                for (int lightIdx = 0; lightIdx < lightCount; lightIdx++)
                {
                    const ForwardLightingShader::Light& light = lights[lightIdx];
                    
                    Vec3 lightDir;
                    float attenuation = 1.0f;
                    
                    if (light.LightType == ForwardLightingShader::Light::DIRECTIONAL)
                    {
                        lightDir.x = -light.Direction.x;
                        lightDir.y = -light.Direction.y;
                        lightDir.z = -light.Direction.z;
                        attenuation = 1.0f;
                    }
                    else
                    {
                        // Point or spot light
                        Vec3 lightVec;
                        lightVec.x = light.Position.x - worldPos.x;
                        lightVec.y = light.Position.y - worldPos.y;
                        lightVec.z = light.Position.z - worldPos.z;
                        float lightLen = sqrtf(lightVec.x * lightVec.x + lightVec.y * lightVec.y + lightVec.z * lightVec.z);
                        if (lightLen > 0.001f)
                        {
                            lightDir.x = lightVec.x / lightLen;
                            lightDir.y = lightVec.y / lightLen;
                            lightDir.z = lightVec.z / lightLen;
                            
                            // Attenuation
                            if (light.Decay > 0.01f)
                            {
                                attenuation = fmaxf(0.0f, 1.0f - lightLen / light.Decay);
                            }
                            
                            // Spot light cone
                            if (light.LightType == ForwardLightingShader::Light::SPOT)
                            {
                                Vec3 spotDir;
                                spotDir.x = -light.Direction.x;
                                spotDir.y = -light.Direction.y;
                                spotDir.z = -light.Direction.z;
                                float spotDot = lightDir.x * spotDir.x + lightDir.y * spotDir.y + lightDir.z * spotDir.z;
                                if (spotDot < light.OuterConeAngle)
                                {
                                    attenuation = 0.0f;
                                }
                                else if (spotDot < light.InnerConeAngle)
                                {
                                    float coneFactor = (spotDot - light.OuterConeAngle) / (light.InnerConeAngle - light.OuterConeAngle);
                                    attenuation *= coneFactor;
                                }
                            }
                        }
                        else
                        {
                            attenuation = 0.0f;
                        }
                    }
                    
                    if (attenuation > 0.001f)
                    {
                        // Diffuse: N·L
                        float NdotL = normal.x * lightDir.x + normal.y * lightDir.y + normal.z * lightDir.z;
                        NdotL = fmaxf(0.0f, NdotL);
                        
                        if (NdotL > 0.0f)
                        {
                            // Diffuse contribution
                            float diffuseContrib = NdotL * attenuation * (1.0f - light.Ambient);
                            color.x += albedo.x * light.Color.x * light.Intensity * diffuseContrib;
                            color.y += albedo.y * light.Color.y * light.Intensity * diffuseContrib;
                            color.z += albedo.z * light.Color.z * light.Intensity * diffuseContrib;
                            
                            // Specular: Blinn-Phong (N·H)^shininess
                            Vec3 halfDir;
                            halfDir.x = lightDir.x + viewDir.x;
                            halfDir.y = lightDir.y + viewDir.y;
                            halfDir.z = lightDir.z + viewDir.z;
                            float halfLen = sqrtf(halfDir.x * halfDir.x + halfDir.y * halfDir.y + halfDir.z * halfDir.z);
                            if (halfLen > 0.001f)
                            {
                                halfDir.x /= halfLen;
                                halfDir.y /= halfLen;
                                halfDir.z /= halfLen;
                                
                                float NdotH = normal.x * halfDir.x + normal.y * halfDir.y + normal.z * halfDir.z;
                                NdotH = fmaxf(0.0f, NdotH);
                                
                                // Approximate power function
                                float specularPower = NdotH;
                                for (int i = 1; i < (int)shininess; i *= 2)
                                {
                                    specularPower *= specularPower;
                                }
                                
                                float specularContrib = specularPower * NdotL * attenuation * light.Intensity;
                                color.x += specularColor.x * light.Color.x * specularContrib;
                                color.y += specularColor.y * light.Color.y * specularContrib;
                                color.z += specularColor.z * light.Color.z * specularContrib;
                            }
                        }
                        
                        // Ambient
                        color.x += light.Color.x * light.Ambient;
                        color.y += light.Color.y * light.Ambient;
                        color.z += light.Color.z * light.Ambient;
                    }
                }
                
                // Clamp and store
                finalR[fragIdx] = fminf(1.0f, fmaxf(0.0f, color.x));
                finalG[fragIdx] = fminf(1.0f, fmaxf(0.0f, color.y));
                finalB[fragIdx] = fminf(1.0f, fmaxf(0.0f, color.z));
                finalA[fragIdx] = albedo.w;
            }
            
            // Output RGBA for 4 fragments
            for (int i = 0; i < 4; i++)
            {
                output[i] = finalR[i];
                output[i + 4] = finalG[i];
                output[i + 8] = finalB[i];
                output[i + 12] = finalA[i];
            }
        }
    };
}

#endif

