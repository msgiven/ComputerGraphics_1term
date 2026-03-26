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
    float4 PosH : SV_POSITION;
    //float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

struct PatchTess
{
    float OuterTess[3] : SV_TessFactor;
    float InnerTess : SV_InsideTessFactor;
    //float InnerTess[2] : SV_InsideTessFactor;
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
    float3 animatedPos = vin.PosL;
    

    float frequency = 0.05f;
    float speed = 3.0f;
    float amplitude = 5.0f; 
    
    float waveOffset = sin(animatedPos.y * frequency + gTotalTime * speed) * amplitude;
    
    animatedPos.x += waveOffset;
    animatedPos.z += waveOffset * 0.5f;

    vout.PosH = mul(float4(animatedPos, 1.0f), gWorldViewProj);

    //vout.PosW = mul(float4(animatedPos, 1.0f), gWorldViewProj).xyz;

    vout.NormalW = vin.NormalL;
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gMatTransform).xy;

    return vout;
}


PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    
    pt.OuterTess[0] = 3;
    pt.OuterTess[1] = 3;
    pt.OuterTess[2] = 3;
  //  pt.OuterTess[3] = 3;
    
    //pt.InnerTess[0] = 3;
   // pt.InnerTess[1] = 3;
    
    pt.InnerTess = 3;
    
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
    
   /* float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x);
    float3 v2 = lerp(quad[1].PosL, quad[2].PosL, uv.x);
    float3 p = lerp(v1, v2, uv.y);
    d.PosH = mul(float4(p, 1.0f), gWorldViewProj);
    
    float3 n1 = lerp(quad[0].NormalW, quad[1].NormalW, uv.x);
    float3 n2 = lerp(quad[1].NormalW, quad[2].NormalW, uv.x);
    d.NormalW = lerp(n1, n2, uv.y); 

    float2 t1 = lerp(quad[0].TexC, quad[1].TexC, uv.x);
    float2 t2 = lerp(quad[1].TexC, quad[2].TexC, uv.x);
    d.TexC = lerp(t1, t2, uv.y);*/
    float3 p = quad[0].PosL * uvw.x + quad[1].PosL * uvw.y + quad[2].PosL * uvw.z;

    float3 n = quad[0].NormalW * uvw.x + quad[1].NormalW * uvw.y + quad[2].NormalW * uvw.z;
    float2 tex = quad[0].TexC * uvw.x + quad[1].TexC * uvw.y + quad[2].TexC * uvw.z;
    
    d.PosH = mul(float4(p, 1.0f), gWorldViewProj);
    d.NormalW = n;
    d.TexC = tex;
    
    return d;
}




float4 PS(DomainOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    diffuseAlbedo.a = 1.0f;
    return diffuseAlbedo;
}