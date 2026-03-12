#ifndef MaxLights
    #define MaxLights 16
#endif

#include "D:\C++Projects\kg_lab4\Shaders\light.hlsl"

// Текстуры G-Buffer
Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gDepthMap : register(t2);

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

    Light gLights[MaxLights];
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


    float4 clipSpacePos = float4(pin.TexC.x * 2.0f - 1.0f, (1.0f - pin.TexC.y) * 2.0f - 1.0f, depth, 1.0f);
    
    float4 worldSpacePos = mul(clipSpacePos, gInvViewProj);
    worldSpacePos /= worldSpacePos.w;
    float3 posW = worldSpacePos.xyz;


    normalW = normalize(normalW);
    float3 toEyeW = normalize(gEyePosW - posW);


    float roughness = 0.5f;
    float3 fresnelR0 = float3(0.02f, 0.02f, 0.02f);
    float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    
    float3 shadowFactor = 1.0f;


    float4 directLight = ComputeLighting(gLights, mat, posW, normalW, toEyeW, shadowFactor);

    float4 litColor = (gAmbientLight * diffuseAlbedo) + directLight;


    litColor.a = diffuseAlbedo.a;

    return litColor;
}