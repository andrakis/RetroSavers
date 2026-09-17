// One generation of Conway's Life. State texel: r = alive (0/1), g = age (grows while alive).
Texture2D gState : register(t0);
SamplerState gSamp : register(s0);   // point; wrap or clamp decides the world's edges

cbuffer Step : register(b0)
{
    float4 gTexel;   // xy = one cell in uv
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_Target
{
    float2 d = gTexel.xy;
    float n = 0.0;
    n += gState.Sample(gSamp, i.uv + float2(-d.x, -d.y)).r;
    n += gState.Sample(gSamp, i.uv + float2(0.0, -d.y)).r;
    n += gState.Sample(gSamp, i.uv + float2(d.x, -d.y)).r;
    n += gState.Sample(gSamp, i.uv + float2(-d.x, 0.0)).r;
    n += gState.Sample(gSamp, i.uv + float2(d.x, 0.0)).r;
    n += gState.Sample(gSamp, i.uv + float2(-d.x, d.y)).r;
    n += gState.Sample(gSamp, i.uv + float2(0.0, d.y)).r;
    n += gState.Sample(gSamp, i.uv + float2(d.x, d.y)).r;
    float4 s = gState.Sample(gSamp, i.uv);
    float alive = s.r > 0.5 ? 1.0 : 0.0;
    float next = (n > 2.5 && n < 3.5) || (alive > 0.5 && n > 1.5 && n < 3.5) ? 1.0 : 0.0;
    float age = next > 0.5 ? min(1.0, s.g + 1.0 / 255.0) : 0.0;
    return float4(next, age, 0.0, 1.0);
}
