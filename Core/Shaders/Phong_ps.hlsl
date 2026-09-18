#include "Common.hlsli"
#include "Lighting.hlsli"

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
    float3 c = ShadePhong(base, n, i.wpos);
    return float4(ApplyFog(c, i.wpos), alpha);
}
