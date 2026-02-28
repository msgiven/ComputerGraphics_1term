#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "D:\C++Projects\kg_lab4\Shaders\light.hlsl"

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
    float2 TexC : TEXCOORD; // 1. Принимаем UV из C++
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD; // 2. Готовим для передачи в пиксельный шейдер
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.PosW = posW.xyz;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.NormalW = vin.NormalL;
    vout.TexC = vin.TexC; // 3. Просто копируем координаты дальше
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // --- Расчет освещения ---
    float3 lightDir = normalize(float3(0.577f, 0.577f, -0.577f));
    float diffuse = max(dot(pin.NormalW, lightDir), 0.0f);
    float ambient = 0.2f;
    
    // --- Работа с "цветом" ---
    // Вместо фиксированного float3(0.8, 0.7, 0.6) используем UV-координаты как цвет.
    // R (красный) будет отвечать за координату U, G (зеленый) — за V.
    float3 texColor = float3(pin.TexC, 0.0f);
    
    // Смешиваем "текстурный" цвет и освещение
    float3 finalColor = texColor * (diffuse + ambient);
    
    return float4(finalColor, 1.0f);
}