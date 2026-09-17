// Aurora (Vista): layered curtains of light. Rendered at half resolution into an HDR target.
cbuffer Params : register(b0)
{
    float4 gTimeRes;   // x = time, y = width, z = height, w = amplitude (0.3 .. 2)
    float4 gParams;    // x = speed (0.3 .. 2), y = brightness (0.3 .. 2), z = layer count, w = seed
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

float vnoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i), b = hash(i + float2(1, 0)), c = hash(i + float2(0, 1)), d = hash(i + float2(1, 1));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm(float2 p)
{
    float v = 0.0, a = 0.5;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        v += a * vnoise(p);
        p = p * 2.03 + float2(17.1, 9.7);
        a *= 0.5;
    }
    return v;
}

float4 main(PSIn i) : SV_Target
{
    float t = gTimeRes.x * gParams.x;
    float amp = gTimeRes.w;
    float2 uv = float2(i.uv.x, 1.0 - i.uv.y);   // y up
    float aspect = gTimeRes.y / max(gTimeRes.z, 1.0);
    float seed = gParams.w;

    // Night sky: deep blue gradient plus a sparse star field.
    float3 col = lerp(float3(0.01, 0.015, 0.05), float3(0.0, 0.0, 0.01), uv.y);
    float2 sp = floor(float2(uv.x * aspect, uv.y) * 220.0);
    float star = hash(sp + seed);
    float twinkle = 0.6 + 0.4 * sin(t * 2.0 + star * 40.0);
    col += (star > 0.994 ? (star - 0.994) * 150.0 : 0.0) * twinkle * 0.6;

    int layers = (int)gParams.z;
    for (int k = 0; k < 10; ++k)
    {
        if (k >= layers) break;
        float fk = (float)k;
        float s = seed + fk * 7.31;
        // Curtain centre line: two summed sines plus slow fbm drift.
        float base = 0.42 + 0.06 * fk / max(1.0, (float)layers) + 0.05 * sin(s);
        float phase = t * (0.12 + 0.03 * fk) + s;
        float center = base + amp * (0.10 * sin(uv.x * 3.1 + phase) + 0.05 * sin(uv.x * 7.3 - phase * 1.7 + s * 2.0))
                     + 0.12 * (fbm(float2(uv.x * 1.6 + s, t * 0.05 + s)) - 0.5);
        float width = 0.035 + 0.075 * (0.5 + 0.5 * sin(s * 3.0 + t * 0.09));
        float d = uv.y - center;
        // Sharp lower edge, soft glow above.
        float sigma = d > 0.0 ? width * 2.2 : width * 0.55;
        float g = exp(-d * d / (2.0 * sigma * sigma));
        // Vertical striations (the curtain "rays").
        float ray = fbm(float2(uv.x * 45.0 + s * 3.0 + t * 0.35 * (0.6 + 0.2 * fk), uv.y * 3.0 + t * 0.05));
        ray = pow(saturate(ray * 1.35), 2.2);
        float flicker = 0.75 + 0.25 * sin(t * 1.3 + uv.x * 9.0 + s);
        float intensity = g * (0.35 + 1.1 * ray) * flicker;

        // Palette: green low, teal/blue mid, violet at the top; drifts slowly with time.
        float hueShift = 0.5 + 0.5 * sin(t * 0.07 + fk * 1.9);
        float3 low = lerp(float3(0.15, 1.0, 0.35), float3(0.1, 0.9, 0.75), hueShift * 0.6);
        float3 high = lerp(float3(0.55, 0.25, 1.0), float3(0.2, 0.4, 1.0), hueShift);
        float up = saturate(d / (width * 3.0));
        float3 c = lerp(low, high, up);
        col += c * intensity * gParams.y * (1.6 / max(1.0, (float)layers)) * 0.9;
    }
    return float4(col, 1.0);
}
