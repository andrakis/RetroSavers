// Final pass: scene + two bloom levels, radial warp blur, chromatic aberration, ACES tone
// curve (which is also what caps the white hole's glare), vignette, fade and dither.
Texture2D gScene : register(t0);
Texture2D gBloom : register(t1);
Texture2D gWide : register(t2);
SamplerState gSamp : register(s0);

cbuffer Composite : register(b0)
{
    float4 gA;   // x = exposure, y = bloom, z = wide bloom, w = chromatic aberration
    float4 gB;   // x = warp (0..1), y = fade, z = time, w = vignette
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float3 SampleCA(float2 uv, float ca)
{
    float2 d = uv - 0.5;
    float r = gScene.Sample(gSamp, 0.5 + d * (1.0 + ca)).r;
    float g = gScene.Sample(gSamp, uv).g;
    float b = gScene.Sample(gSamp, 0.5 + d * (1.0 - ca)).b;
    return float3(r, g, b);
}

float3 Aces(float3 x)
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

float4 main(PSIn i) : SV_Target
{
    float2 uv = i.uv;
    float ca = gA.w;
    float warp = gB.x;
    float3 c = SampleCA(uv, ca);
    float3 bloom = gBloom.Sample(gSamp, uv).rgb;
    float3 wide = gWide.Sample(gSamp, uv).rgb;

    // Radial blur towards the centre during a warp jump.
    [branch]
    if (warp > 0.001)
    {
        float2 d = uv - 0.5;
        float3 acc = c;
        float wsum = 1.0;
        // Per-pixel jitter hides the discrete sample steps.
        float jit = frac(sin(dot(i.pos.xy, float2(12.9898, 78.233))) * 43758.5453);
        [unroll]
        for (int k = 1; k <= 14; ++k)
        {
            float s = 1.0 - warp * 0.22 * (k - jit) / 14.0;
            float wk = 1.0 - k / 15.0;
            acc += SampleCA(0.5 + d * s, ca + warp * 0.02) * wk;
            wsum += wk;
        }
        c = acc / wsum;
    }

    float3 hdr = (c + bloom * gA.y + wide * gA.z) * gA.x;
    float3 col = pow(Aces(hdr), 1.0 / 2.2);   // the swap chain is UNORM, not sRGB
    float2 v = uv - 0.5;
    col *= 1.0 - gB.w * dot(v, v) * 1.6;
    col *= gB.y;
    // Dither against banding in the bloom gradients.
    float n = frac(sin(dot(i.pos.xy + gB.z, float2(12.9898, 78.233))) * 43758.5453);
    col += (n - 0.5) / 255.0;
    return float4(col, 1.0);
}
