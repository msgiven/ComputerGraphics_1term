#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#include "D:\C++Projects\kg_lab4\Shaders\light.hlsl"

Texture2D gDiffuseMap : register(t0);
SamplerState gsamAnisotropicWrap : register(s4);

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

    // Light gLights[16]; 
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
    float gDispScale;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float3 PosL : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

struct PatchTess
{
    float OuterTess[3] : SV_TessFactor;
    float InnerTess : SV_InsideTessFactor;
};

struct HullOut
{
    float3 PosL : POSITION; 
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

struct DomainOut
{
    float4 PosH : SV_POSITION; 
    float3 PosW : POSITION; 
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;
    vout.NormalW = vin.NormalL;
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gMatTransform).xy;
    return vout;
}

PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    pt.OuterTess[0] = 30.0f;
    pt.OuterTess[1] = 30.0f;
    pt.OuterTess[2] = 30.0f;
    pt.InnerTess = 30.0f;
    return pt;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 3> patch, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    HullOut h;
    h.PosL = patch[i].PosL;
    h.NormalW = patch[i].NormalW;
    h.TexC = patch[i].TexC;
    return h;
}

[domain("tri")]
DomainOut DS(PatchTess patchTess, float3 uvw : SV_DomainLocation, const OutputPatch<HullOut, 3> quad)
{
    DomainOut d;
    
    float3 p = quad[0].PosL * uvw.x + quad[1].PosL * uvw.y + quad[2].PosL * uvw.z;

    float waveSpeed = 2.0f;
    float waveHeight = 1.5f; 
    float frequency = 0.5f;
    
    p.y += sin(p.x * frequency + gTotalTime * waveSpeed) * waveHeight;
    p.y += cos(p.z * frequency * 0.8f + gTotalTime * waveSpeed * 1.1f) * waveHeight * 0.5f;


    d.PosH = mul(float4(p, 1.0f), gWorldViewProj);
    
    //d.PosW = mul(float4(p, 1.0f), gWorld).xyz; 

    d.NormalW = normalize(quad[0].NormalW * uvw.x + quad[1].NormalW * uvw.y + quad[2].NormalW * uvw.z);
    d.TexC = quad[0].TexC * uvw.x + quad[1].TexC * uvw.y + quad[2].TexC * uvw.z;
    
    return d;
}

float4 PS(DomainOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    
    diffuseAlbedo.a = 0.5f; 
    
    return diffuseAlbedo;
}