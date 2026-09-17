// Draws the Life state as coloured cells with a faint grid.
Texture2D gState : register(t0);
SamplerState gSamp : register(s0);   // point

cbuffer Show : register(b0)
{
    float4 gParams;   // x = palette, y = cells across, z = cells down, w = grid strength
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float3 Hue(float h)
{
    return saturate(abs(frac(h + float3(0.0, 0.667, 0.333)) * 6.0 - 3.0) - 1.0);
}

float4 main(PSIn i) : SV_Target
{
    float4 s = gState.Sample(gSamp, i.uv);
    float alive = s.r > 0.5 ? 1.0 : 0.0;
    float age = saturate(s.g * 3.0);  // newborn .. settled (~85 generations)
    float3 c;
    int palette = (int)gParams.x;
    if (palette == 1)      c = Hue(0.9 - age * 0.9) * (0.6 + 0.4 * (1.0 - age));          // hues by age
    else if (palette == 2) c = float3(1.0, 1.0, 1.0) * (1.0 - 0.6 * age);                 // white fading grey
    else if (palette == 3) c = float3(1.0, 0.7, 0.2) * (1.0 - 0.5 * age);                 // amber terminal
    else                   c = lerp(float3(0.5, 1.0, 0.5), float3(0.05, 0.45, 0.1), age);  // classic green
    c *= alive;
    // Grid lines between cells.
    float2 f = frac(i.uv * gParams.yz);
    float grid = (f.x < 0.08 || f.y < 0.08) ? 1.0 : 0.0;
    c = lerp(c, float3(0.06, 0.06, 0.08), grid * gParams.w);
    return float4(c, 1.0);
}
