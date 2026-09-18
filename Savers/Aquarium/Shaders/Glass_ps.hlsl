// Final composite over the rendered tank: a vignette at the corners and a faint diagonal
// reflection band, as if seen through the front glass. Alpha-blended over the swap chain.

cbuffer Glass : register(b0)
{
    float4 gParams;   // x = time, y = aspect, z = vignette strength, w = reflection strength
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float2 d = i.uv - 0.5;
    float r = length(d * float2(1.0, 0.85));
    float vig = smoothstep(0.42, 0.9, r) * gParams.z;

    // Two soft bands sloping down to the right, drifting very slowly.
    float s = i.uv.x * gParams.y * 0.55 + i.uv.y * 0.75 + 0.03 * sin(gParams.x * 0.05);
    float band = 0.6 * exp(-pow((s - 0.62) / 0.05, 2.0)) + 0.35 * exp(-pow((s - 0.38) / 0.16, 2.0));
    float refl = band * gParams.w;

    float a = saturate(vig + refl);
    float3 rgb = float3(1.0, 1.0, 1.0) * (refl / max(vig + refl, 1e-3));
    return float4(rgb, a);
}
