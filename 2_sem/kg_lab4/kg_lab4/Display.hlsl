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
Texture2DArray gShadowMap : register(t4);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerComparisonState gsamBorder : register(s6);

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

struct CascadeData
{
    float4x4 lightViewProj;
    float4x4 shadowTran;
    float4 distances;
    float4 padding[7];
};

cbuffer ShadowCB : register(b1)
{
    CascadeData gCascades[3];
    uint gCascadeIndex;
    float3 padding_end;
}

cbuffer cbPerObject : register(b2)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;
};

struct ShadowVertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentL : TANGENT;
};

struct ShadowVertexOut
{
    float4 PosH : SV_POSITION;
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

ShadowVertexOut VS_Shadow(ShadowVertexIn vin)
{
    ShadowVertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gCascades[gCascadeIndex].lightViewProj);
    return vout;
}


float ComputeShadowPCF(float3 posW)
{
    float4 viewPos = mul(float4(posW, 1.0f), gView);

    float depthVS = viewPos.z;
    uint cascadeIndex = 0;
    [unroll]
    for (uint j = 0; j < 2; j++)
    {
        if (depthVS > gCascades[j].distances[j])
        {
            cascadeIndex = j + 1;
        }
    }
    cascadeIndex = min(cascadeIndex, 2u);


    float shadowFactor = 1.0f;

    if (cascadeIndex < 3)
    {
        float4 shadowPosH = mul(float4(posW, 1.0f), gCascades[cascadeIndex].shadowTran);
        shadowPosH.xyz /= shadowPosH.w;
        shadowFactor = gShadowMap.SampleCmpLevelZero(
            gsamBorder,
            float3(shadowPosH.xy, (float) cascadeIndex),
            shadowPosH.z
        );
    }
    return shadowFactor;

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
    
    float3 directLightColor = 0.0f;

    uint i = 0;
    for (i = 0; i < gNumDirLights && i < gNumLightsTotal; ++i)
    {
        float shadow = ComputeShadowPCF(posW);
        directLightColor += ComputeDirectionalLight(gLights[i], mat, normalW, toEyeW) * shadow;
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

    litColor.a = diffuseAlbedo.a;
    return litColor;
}
