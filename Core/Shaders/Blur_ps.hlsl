Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

cbuffer Blur : register(b0)
{
    float4 gStep; // xy = one-texel step along the blur direction
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

// 9-tap separable Gaussian (sigma ~ 2.5 texels).
float4 main(PSIn i) : SV_Target
{
    static const float w[5] = { 0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162 };
    float4 c = gTex.Sample(gSamp, i.uv) * w[0];
    [unroll]
    for (int k = 1; k < 5; ++k)
    {
        float2 d = gStep.xy * k * 1.5;
        c += gTex.Sample(gSamp, i.uv + d) * w[k];
        c += gTex.Sample(gSamp, i.uv - d) * w[k];
    }
    return c;
}
