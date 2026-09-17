// Sprite2D pixel shader override: each rug cell becomes a round dot on black, like the
// Windows 3.x rendering that plotted the pattern with spaced points.
Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);   // point sampling; wrap for the tiled layout

cbuffer Dots : register(b0)
{
    float4 gParams;   // x = cells per uv unit, y = dot radius (cell units), z = pixels per cell
};

struct PSIn
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSIn i) : SV_Target
{
    float2 cell = i.uv * gParams.x;
    float2 f = frac(cell) - 0.5;
    float d = length(f);
    float aa = 0.7 / max(gParams.z, 1.0);   // ~one pixel of anti-aliasing in cell units
    float m = 1.0 - smoothstep(gParams.y - aa, gParams.y + aa, d);
    float2 centre = (floor(cell) + 0.5) / gParams.x;
    float4 c = gTex.Sample(gSamp, centre) * i.color;
    return float4(c.rgb * m, 1.0);
}
