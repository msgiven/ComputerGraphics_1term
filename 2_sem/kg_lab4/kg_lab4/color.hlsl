#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 1
#endif

// Include structures and functions for lighting.
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
    //float4x4 gTexTransform;
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

    Light gLights[MaxLights];
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

struct GBufferOut
{
    float4 Diffuse : SV_Target0;
    float4 Normal : SV_Target1;
    float Pos : SV_Target2;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.PosW = posW.xyz;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.NormalW = vin.NormalL;
    vout.TexC = vin.TexC; 
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), float4(1.0f, 1.0f, 1.0f, 1.0f));
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gMatTransform).xy;
    return vout;
}

GBufferOut PS(VertexOut pin) : SV_Target
{
    GBufferOut gbuf;
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    gbuf.Diffuse = diffuseAlbedo;

    pin.NormalW = normalize(pin.NormalW);
    gbuf.Normal = float4(pin.NormalW, 0.0f);
    gbuf.Pos = pin.PosH.z / pin.PosH.w;
 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);


    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;


    litColor.a = diffuseAlbedo.a;

    //return litColor;
    return gbuf;
}