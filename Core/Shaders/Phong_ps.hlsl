#include "Common.hlsli"

Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

float4 main(PhongPSIn i) : SV_Target
{
    float3 n = normalize(i.nrm);
    float3 base = i.color.rgb;
    float alpha = i.color.a;
    if (gMaterial.z > 0.5)
    {
        float4 t = gTex.Sample(gSamp, i.uv);
        base *= t.rgb;
        alpha *= t.a;
    }
    float3 L = normalize(gLightDir.xyz);
    float ndl = saturate(dot(n, L));
    float3 V = normalize(gEyePos.xyz - i.wpos);
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(n, H)), max(gMaterial.x, 1.0)) * gMaterial.y * (ndl > 0.0 ? 1.0 : 0.0);
    float3 c = base * (gAmbient.rgb + gLightColor.rgb * ndl) + gLightColor.rgb * spec;
    if (gFogColor.a > 0.5)
    {
        float d = distance(gEyePos.xyz, i.wpos);
        float f = saturate((d - gFogParams.x) / max(gFogParams.y - gFogParams.x, 0.001));
        c = lerp(c, gFogColor.rgb, f);
    }
    return float4(c, alpha);
}
