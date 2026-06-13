
Texture2D pSceneMap : register(t0);
Texture2D pDepthMap : register(t1);
SamplerState pSamPointWrap : register(s0);

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


float GetLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

float3 GetBlurColor(float2 uvCoord, float screenHeight)
{
    float4 blurColor = float4(0, 0, 0, 0);

    float2 texelSize = float2(0.0f, 1.0f / screenHeight);

    float count = 0;
[unroll]
    for (int i = -5; i <= 5; i++)
    {
        float2 offset = texelSize * i;
        blurColor += pSceneMap.SampleLevel(pSamPointWrap, uvCoord + offset, 0);
        count += 1.0f;
    }
    return blurColor.rgb / count;
}

float GetVignette(float2 uv)
{
    float2 ndc = 2.0f * uv - 1.0f;
    float vignetteIntensity = 0.7f;
    float vignetteMinBoard = 0.6f;
    float vignetteMaxBoard = 1.35f;
    float len = length(ndc);
    float vignette = 1.0f - smoothstep(vignetteMinBoard, vignetteMaxBoard, len) * vignetteIntensity;
    return vignette;
}

float random(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float4 PS(VertexOut pin) : SV_Target
{
    
    float4 originalColor = pSceneMap.Sample(pSamPointWrap, pin.TexC) *2.0f;
    return originalColor;
    /*float width, height;
    pDepthMap.GetDimensions(width, height);
    float depth = pDepthMap.Load(int3(pin.TexC.x * width, pin.TexC.y * height, 0)).r;
    
    
    pSceneMap.GetDimensions(width, height);
    float3 blurredColor = GetBlurColor(pin.TexC, height);

    float blurStart = 0.97; 
    float blurEnd = 0.999;
    float t = saturate((depth - blurStart) / (blurEnd - blurStart));
    float blurFactor = pow(t, 2.0);
    float grayFactor = min(blurFactor + 0.5, 1.0f);
    float3 lumOrig = GetLuminance(blurredColor.rgb);
    float grayScaaleR = lerp(blurredColor.r, lumOrig.r, grayFactor);
    float grayScaaleG = lerp(blurredColor.g, lumOrig.g, grayFactor);
    float grayScaaleB = lerp(blurredColor.b, lumOrig.b, grayFactor);
    blurredColor = float4(grayScaaleR, grayScaaleG, grayScaaleB, 1.0f);
    float3 blurredMixed = lerp(originalColor.rgb, blurredColor, blurFactor);
    
    float l00 = GetLuminance(pSceneMap.Sample(pSamPointWrap, pin.TexC, int2(-1, -1)).rgb);
    float l01 = GetLuminance(pSceneMap.Sample(pSamPointWrap, pin.TexC, int2(0, -1)).rgb);
    float l02 = GetLuminance(pSceneMap.Sample(pSamPointWrap, pin.TexC, int2(1, -1)).rgb);
    
    float l10 = GetLuminance(pSceneMap.Sample(pSamPointWrap, pin.TexC, int2(-1, 0)).rgb);
    // l11 center
    float l12 = GetLuminance(pSceneMap.Sample(pSamPointWrap, pin.TexC, int2(1, 0)).rgb);
    
    float l20 = GetLuminance(pSceneMap.Sample(pSamPointWrap, pin.TexC, int2(-1, 1)).rgb);
    float l21 = GetLuminance(pSceneMap.Sample(pSamPointWrap, pin.TexC, int2(0, 1)).rgb);
    float l22 = GetLuminance(pSceneMap.Sample(pSamPointWrap, pin.TexC, int2(1, 1)).rgb);


     float Gx = (-1.0f * l00) + (1.0f * l02) +
               (-2.0f * l10) + (2.0f * l12) +
               (-1.0f * l20) + (1.0f * l22);


    float Gy = (-1.0f * l00) + (-2.0f * l01) + (-1.0f * l02) +
               (1.0f * l20) + (2.0f * l21) + (1.0f * l22);

    float edgeMagnitude = sqrt(Gx * Gx + Gy * Gy);

    float edgeThreshold = 0.5f; 
    float edge = step(edgeThreshold, edgeMagnitude); 
    float edgeMask = lerp(1.0 - edge, 1.0, blurFactor);
    float3 finalColor = blurredMixed * edgeMask;
    
    float noise = random(pin.TexC);
    float grainIntensity = 0.025;
    finalColor = saturate(finalColor + noise * grainIntensity);
    finalColor *= GetVignette(pin.TexC);
    return float4(finalColor, 1.0f);*/


}