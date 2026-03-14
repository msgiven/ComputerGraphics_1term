#ifndef MaxLights
    #define MaxLights 1300
#endif
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 2
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 1
#endif

#include "D:\C++Projects\kg_lab4\Shaders\light.hlsl"

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gDepthMap : register(t2);
StructuredBuffer<Light> gLights : register(t3);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamAnisotropicWrap : register(s4);

cbuffer cbPass : register(b0)
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
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    

    vout.TexC = float2((vid << 1) & 2, vid & 2);
    vout.PosH = float4(vout.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamPointWrap, pin.TexC);
    float3 normalW = gNormalMap.Sample(gsamPointWrap, pin.TexC).xyz;
    float depth = gDepthMap.Sample(gsamPointWrap, pin.TexC).r;

    if (depth >= 1.0f)
    {
        discard; 
    }


    float2 ndcXY = float2(pin.TexC.x * 2.0f - 1.0f, (1.0f - pin.TexC.y) * 2.0f - 1.0f);
    float4 clipSpacePos = float4(ndcXY, depth, 1.0f);


    float4 worldPos = mul(clipSpacePos, gInvViewProj);
    float3 posW = worldPos.xyz / worldPos.w;


    normalW = normalize(normalW);
    float3 toEyeW = normalize(gEyePosW - posW);


    float roughness = 0.5f;
    float3 fresnelR0 = float3(0.02f, 0.02f, 0.02f);
    float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    
    float3 shadowFactor = 1.0f;

    float3 directLightColor = 0.0f;

    uint i = 0;
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
        return float4(0.0f, 0.5f, 1.0f, 1.0f); // голубой*/
    litColor.a = diffuseAlbedo.a;

   return litColor;
}