#include "D:\C++Projects\kg_lab4\Shaders\light.hlsl"


Texture2D gDiffuse : register(t0);
Texture2D gNormal : register(t1);
Texture2D gPosition : register(t2); 
SamplerState gSam : register(s0);



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

struct VS_OUT
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

// Генерация полноэкранного треугольника (без буфера вершин)
VS_OUT VS(uint vid : SV_VertexID)
{
    VS_OUT vout;
    vout.TexC = float2((vid << 1) & 2, vid & 2);
    vout.PosH = float4(vout.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return vout;
}

float4 PS(VS_OUT pin) : SV_Target
{
    // 1. Достаем данные из G-Buffer
    float3 worldPos = gPosition.Sample(gSam, pin.TexC).xyz;
    float3 normal = gNormal.Sample(gSam, pin.TexC).xyz;
    float4 diffuse = gDiffuse.Sample(gSam, pin.TexC);


    if (length(normal) < 0.1f)
        return float4(0.2f, 0.2f, 0.3f, 1.0f);

    normal = normalize(normal);


    float3 toEye = normalize(gEyePosW - worldPos);


    float3 lightVec = -gLightDirection;
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightColor = (gAmbientLight.rgb + ndotl * gLightStrength.rgb) * diffuse.rgb;

    float3 halfVec = normalize(toEye + lightVec);
    float spec = pow(max(dot(normal, halfVec), 0.0f), 32.0f);
    lightColor += spec * 0.5f;

    return float4(lightColor, 1.0f);
}