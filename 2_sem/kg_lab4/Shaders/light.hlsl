#define MaxLights 16
static const float PI = 3.14159265359f;

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
};

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}


float3 fresnelSchlick(float cosTheta, float3 F0)
{
    float byak5 = clamp(1.0f - cosTheta, 0.0f, 1.0f) * clamp(1.0f - cosTheta, 0.0f, 1.0f) * clamp(1.0f - cosTheta, 0.0f, 1.0f) * clamp(1.0f - cosTheta, 0.0f, 1.0f) * clamp(1.0f - cosTheta, 0.0f, 1.0f);
    return F0 + (1.0f - F0) * byak5;
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 r = float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness);
    float3 maxVal = max(r, F0);
    
    float byak5 = clamp(1.0f - cosTheta, 0.0f, 1.0f);
    byak5 = byak5 * byak5 * byak5 * byak5 * byak5;
    
    return F0 + (maxVal - F0) * byak5;
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float num = NdotV;
    float denom = NdotV * (1.0f - k) + k;
    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float DistributionBeckmann(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    
    float cosTheta = max(dot(N, H), 0.0f);
    float cosTheta2 = cosTheta * cosTheta;
    float cosTheta4 = cosTheta2 * cosTheta2;
    
    float tan2Theta = (1.0f - cosTheta2) / cosTheta2;
    float num = exp(-tan2Theta / a2);
    float denom = PI * a2 * cosTheta4;
    
    return num / max(denom, 0.0000001f);
}

float3 ComputePBR(float3 radiance, float3 lightVec, float3 normal, float3 toEye, Material mat, float isGGX)
{
    float3 N = normalize(normal);
    float3 V = normalize(toEye);
    float3 L = normalize(lightVec);
    float3 H = normalize(V + L);

    float NDF = 0.0f;
    if (isGGX >= 1.0f)
    {
        NDF = DistributionGGX(N, H, mat.Roughness);
    }
    else
    {
        NDF = DistributionBeckmann(N, H, mat.Roughness);
    }

    float G = GeometrySmith(N, V, L, mat.Roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0f), mat.FresnelR0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
    float3 specular = numerator / denominator;
    

    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    
    float NdotL = max(dot(N, L), 0.0f);
    
    float3 diffuse = mat.DiffuseAlbedo.rgb / PI;

    return (kD * diffuse + specular) * radiance * NdotL;
}


float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye, float isGGX)
{
    float3 lightVec = -L.Direction;

    return ComputePBR(L.Strength, lightVec, normal, toEye, mat, isGGX);
}


float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye, float isGGX)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;
    lightVec /= d;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    float3 radiance = L.Strength * att;

    return ComputePBR(radiance, lightVec, normal, toEye, mat, isGGX);
}


float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye, float isGGX)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;
    lightVec /= d;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    float3 radiance = L.Strength * att * spotFactor;

    return ComputePBR(radiance, lightVec, normal, toEye, mat, isGGX);
}

/*float4 ComputeLighting(Light gLights[MaxLights], Material mat,
                       float3 pos, float3 normal, float3 toEye,
                       float3 shadowFactor)
{
    float3 result = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; ++i)
    {
        result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif 

    return float4(result, 0.0f);
}*/


