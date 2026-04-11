struct GrassInstanceData
{
    float3 Position;
    float Scale;
};

StructuredBuffer<GrassInstanceData> gInstanceData : register(t0);

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
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float Height : TEXCOORD0;
};

VertexOut VS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VertexOut vout;
    GrassInstanceData inst = gInstanceData[iid];

    float bladeCurvature = 2.0f; 
    float grassWidth = 0.2f; 
    float grassHeight = 1.5f;


    float v = float(vid & 1);
    float u = float(vid >> 1) * 2.0f - 1.0f;
    vout.Height = v;
    float t = v;
    float widthScale = 1.0f - pow(t, bladeCurvature);
    

    float3 posL;
    posL.x = u * grassWidth * widthScale;
    posL.y = v * grassHeight;
    posL.z = 0.0f;
    
    posL *= inst.Scale;

    float3 posW = posL + inst.Position;

    vout.PosW = posW;
    vout.PosH = mul(float4(posW, 1.0f), gViewProj);
    vout.NormalW = float3(0.0f, 0.0f, -1.0f);

    return vout;
}

struct PSOut
{
    float4 DiffRTV : SV_Target0;
    float4 NormalRTV : SV_Target1;
    float4 PosRTV : SV_Target2;
};

PSOut PS(VertexOut pin)
{
    
    PSOut pout;
    float4 baseColor = float4(0.13f, 0.2f, 0.13f, 1.0f);
    float4 tipColor = float4(0.13f, 0.85f, 0.13f, 1.0f);
    float4 col = lerp(baseColor, tipColor, pin.Height);

    pout.DiffRTV = col;
    pout.NormalRTV = float4(pin.NormalW, 0.0f);
    pout.PosRTV = float4(pin.PosW, 1.0f);

    return pout;
}