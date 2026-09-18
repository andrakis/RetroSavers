// Blinn-Phong shading shared by Phong_ps and saver pixel shaders that bring their own
// surface colour (include after Common.hlsli). The projected light map in t1 (C13) is a
// top-down texture indexed by world XZ that brightens the diffuse term - caustics, spotlights.

#ifndef RS_LIGHTING_HLSLI
#define RS_LIGHTING_HLSLI

Texture2D gLightMapTex : register(t1);
SamplerState gLightMapSamp : register(s1);

// Light-map factor at a world position: 1 when disabled, 1 + strength * texel otherwise.
float LightMapFactor(float3 wpos)
{
    float f = 1.0;
    if (gLightMap.x > 0.5) f += gLightMap.z * gLightMapTex.Sample(gLightMapSamp, wpos.xz * gLightMap.y).r;
    return f;
}

// Lit colour for a base albedo; specPower / specIntensity default to gMaterial.xy.
float3 ShadePhong(float3 base, float3 n, float3 wpos, float specPower, float specIntensity)
{
    float3 L = normalize(gLightDir.xyz);
    float ndl = saturate(dot(n, L));
    float3 V = normalize(gEyePos.xyz - wpos);
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(n, H)), max(specPower, 1.0)) * specIntensity * (ndl > 0.0 ? 1.0 : 0.0);
    float lm = LightMapFactor(wpos);
    return base * (gAmbient.rgb + gLightColor.rgb * ndl * lm) + gLightColor.rgb * spec * lm;
}

float3 ShadePhong(float3 base, float3 n, float3 wpos)
{
    return ShadePhong(base, n, wpos, gMaterial.x, gMaterial.y);
}

// Distance fog towards gFogColor when enabled.
float3 ApplyFog(float3 c, float3 wpos)
{
    if (gFogColor.a > 0.5)
    {
        float d = distance(gEyePos.xyz, wpos);
        float f = saturate((d - gFogParams.x) / max(gFogParams.y - gFogParams.x, 0.001));
        c = lerp(c, gFogColor.rgb, f);
    }
    return c;
}

#endif
