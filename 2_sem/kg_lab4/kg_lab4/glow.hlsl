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
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.TexC = vin.TexC;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float2 centeredUv = pin.TexC * 2.0f - 1.0f;
    float radial = saturate(1.0f - dot(centeredUv, centeredUv));
    float glowAtten = 0.30f + radial * radial * 2.70f;

    float3 emissive = gDiffuseAlbedo.rgb * glowAtten;
    
    return float4(emissive, 1.0f);
}