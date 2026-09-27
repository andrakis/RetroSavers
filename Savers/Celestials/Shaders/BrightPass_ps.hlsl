// Downsample with a soft-knee threshold: only the hot parts of the scene feed the bloom.
Texture2D gScene : register(t0);
SamplerState gSamp : register(s0);

cbuffer Bright : register(b0)
{
    float4 gParams;   // x = threshold, y = knee, zw = source texel size
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float2 o = gParams.zw * 0.5;
    float3 c = gScene.Sample(gSamp, i.uv + float2(-o.x, -o.y)).rgb
             + gScene.Sample(gSamp, i.uv + float2( o.x, -o.y)).rgb
             + gScene.Sample(gSamp, i.uv + float2(-o.x,  o.y)).rgb
             + gScene.Sample(gSamp, i.uv + float2( o.x,  o.y)).rgb;
    c *= 0.25;
    float peak = max(c.r, max(c.g, c.b));
    float knee = gParams.y;
    float soft = clamp(peak - gParams.x + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    float w = max(soft, peak - gParams.x) / max(peak, 1e-4);
    return float4(c * w, 1.0);
}
