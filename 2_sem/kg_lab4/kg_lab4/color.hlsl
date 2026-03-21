#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 2
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
    
    uint gNumDirLights;
    uint gNumPointLights;
    uint gNumSpotLights;
    uint gNumLightsTotal;

    //Light gLights[MaxLights];
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
    float3 PosL : POSITION;
    float4 PosH : SV_POSITION;
  //  float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD; 
};

struct GBufferOut
{
    float4 Diffuse : SV_Target0;
    float4 Normal : SV_Target1;
    float Pos : SV_Target2;
};

struct PatchTess
{
    float OuterTess[4] : SV_TessFactor;
    float InnerTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
    float3 PosL : POSITIONT;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

struct DomainOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;
   // float4 posW = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
   // vout.PosW = posW.xyz;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.NormalW = vin.NormalL;
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gMatTransform).xy;
    return vout;
}


PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    
    pt.OuterTess[0] = 3;
    pt.OuterTess[1] = 3;
    pt.OuterTess[2] = 3;
    pt.OuterTess[3] = 3;
    
    pt.InnerTess[0] = 3;
    pt.InnerTess[1] = 3;
    
    return pt;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 4> patch, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    HullOut h;
    h.PosL = patch[i].PosL;
    h.NormalW = patch[i].NormalW;
    h.TexC = patch[i].TexC;
    return h;
}

[domain("quad")]
DomainOut DS(PatchTess patchTess, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 4> quad)
{
    DomainOut d;
    
    float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x);
    float3 v2 = lerp(quad[2].PosL, quad[3].PosL, uv.x);
    float3 p = lerp(v1, v2, uv.y);
    d.PosH = mul(float4(p, 1.0f), gWorldViewProj);
    
    float3 n1 = lerp(quad[0].NormalW, quad[1].NormalW, uv.x);
    float3 n2 = lerp(quad[2].NormalW, quad[3].NormalW, uv.x);
    d.NormalW = lerp(n1, n2, uv.y); 

    float2 t1 = lerp(quad[0].TexC, quad[1].TexC, uv.x);
    float2 t2 = lerp(quad[2].TexC, quad[3].TexC, uv.x);
    d.TexC = lerp(t1, t2, uv.y);
    
    return d;
}


GBufferOut PS(DomainOut pin) : SV_Target
{
    GBufferOut gbuf;
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    gbuf.Diffuse = diffuseAlbedo;

    pin.NormalW = normalize(pin.NormalW);
    gbuf.Normal = float4(pin.NormalW, 0.0f);
    gbuf.Pos = pin.PosH.z;

    return gbuf;
}