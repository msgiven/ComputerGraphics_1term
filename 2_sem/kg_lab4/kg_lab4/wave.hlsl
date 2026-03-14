#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "D:\C++Projects\kg_lab4\Shaders\light.hlsl"

Texture2D gDiffuseMap : register(t0);
SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    
    uint gNumDirLights;
    uint gNumPointLights;
    uint gNumSpotLights;
    uint gNumLightsTotal;

   // Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
   
    float3 animatedPos = vin.PosL;
    

    float frequency = 0.05f;
    float speed = 3.0f;
    float amplitude = 5.0f; 
    
    float waveOffset = sin(animatedPos.y * frequency + gTotalTime * speed) * amplitude;
    
    animatedPos.x += waveOffset;
    animatedPos.z += waveOffset * 0.5f;

    vout.PosH = mul(float4(animatedPos, 1.0f), gWorldViewProj);

    vout.PosW = mul(float4(animatedPos, 1.0f), gWorldViewProj).xyz;

    vout.NormalW = vin.NormalL;
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gMatTransform).xy;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    diffuseAlbedo.a = 1.0f;
    /*pin.NormalW = normalize(pin.NormalW);
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float3 directLightColor = 0.0f;
    for (i = 0; i < gNumDirLights && i < gNumLightsTotal; ++i)
    {
        directLightColor += ComputeDirectionalLight(gLights[i], mat, normalW, toEyeW);
    }

    for (; i < gNumDirLights + gNumPointLights && i < gNumLightsTotal; ++i)
    {
        directLightColor += ComputePointLight(gLights[i], mat, posW, normalW, toEyeW);
    }

    for (; i < gNumLightsTotal; ++i)
    {
        directLightColor += ComputeSpotLight(gLights[i], mat, posW, normalW, toEyeW);
    }

    float4 directLight = float4(directLightColor, 0.0f);

    float4 litColor = (gAmbientLight * diffuseAlbedo) + directLight;
    float3 debugColor = float3(0, 0, 0);

    /*float d1 = length(posW - gLights[1].Position);
    if (d1 < 50.0f)
        return float4(1.0f, 0.5f, 0.0f, 1.0f); // оранжевый

    float d2 = length(posW - gLights[2].Position);
    if (d2 < 50.0f)
        return float4(0.0f, 0.5f, 1.0f, 1.0f); // голубой
    litColor.a = diffuseAlbedo.a;*/

    return diffuseAlbedo;
}