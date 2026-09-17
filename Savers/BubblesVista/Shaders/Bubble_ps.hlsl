// Sprite2D pixel shader override: a soap bubble in a quad. The sprite colour carries per-bubble
// data: r = film phase, a = opacity. Output is premultiplied (use PremultipliedAlpha blend).
Texture2D gTex : register(t0);        // sprite texture (white); unused
Texture2D gDesktop : register(t1);    // desktop under this viewport when available
SamplerState gSamp : register(s0);

cbuffer Bubble : register(b0)
{
    float4 gRect;     // viewport left, top, width, height in render-target pixels
    float4 gParams;   // x = has desktop, y = refraction strength (uv units), z = colour mode, w = unused
    float4 gLight;    // xyz = direction towards the light, w = specular power
    float4 gTint;     // rgb = solid colour
};

struct PSIn
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float3 Film(float t)
{
    return 0.5 + 0.5 * cos(6.2831853 * (t + float3(0.0, 0.33, 0.67)));
}

float4 main(PSIn i) : SV_Target
{
    float2 d = i.uv * 2.0 - 1.0;
    d.y = -d.y;
    float r2 = dot(d, d);
    if (r2 > 1.0) discard;
    float3 n = float3(d, sqrt(1.0 - r2));
    float fres = 0.04 + 0.96 * pow(1.0 - n.z, 4.0);
    float phase = i.color.r;

    float3 tint;
    if (gParams.z < 0.5) tint = Film(n.z * 1.6 + phase);          // iridescent bands across the film
    else if (gParams.z < 1.5) tint = gTint.rgb;                    // one colour
    else tint = Film(phase) * 0.6 + 0.4;                           // a hue per bubble

    float3 L = normalize(gLight.xyz);
    float spec = pow(saturate(dot(n, L)), gLight.w);
    float spec2 = pow(saturate(dot(n, normalize(float3(0.5, -0.6, 0.6)))), gLight.w * 2.0) * 0.35;

    float a = saturate(0.08 + 0.92 * fres) * i.color.a;   // film is thin in the middle, strong at the rim
    float edge = 1.0 - smoothstep(0.94, 1.0, sqrt(r2));
    float3 rgb = tint * a * 0.9;
    float aTotal = a;
    if (gParams.x > 0.5)
    {
        // Refract the desktop through the bubble: offset along the surface normal.
        float2 suv = (i.pos.xy - gRect.xy) / gRect.zw;
        float2 ruv = suv + n.xy * gParams.y * (1.0 - n.z);
        float3 refr = gDesktop.Sample(gSamp, ruv).rgb;
        float k = 0.55 * (1.0 - a) * i.color.a;
        rgb += refr * k;
        aTotal = saturate(a + k);
    }
    rgb += (spec + spec2) * i.color.a;
    return float4(rgb * edge, aTotal * edge);
}
