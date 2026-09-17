// Composite: base + bloom, tonemap, vignette, dither.
Texture2D gBase : register(t0);
Texture2D gBloom : register(t1);
SamplerState gSamp : register(s0);

cbuffer Params : register(b0)
{
    float4 gParams;   // x = bloom strength, y = vignette, z = exposure, w = time
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float hash(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float4 main(PSIn i) : SV_Target
{
    float3 c = gBase.Sample(gSamp, i.uv).rgb + gBloom.Sample(gSamp, i.uv).rgb * gParams.x;
    c = 1.0 - exp(-c * gParams.z);
    float2 d = i.uv - 0.5;
    c *= 1.0 - gParams.y * dot(d, d) * 2.0;
    c += (hash(i.pos.xy + gParams.w) - 0.5) / 255.0;
    return float4(saturate(c), 1.0);
}
