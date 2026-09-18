// Animated, tileable caustics: two layers of wrapped Voronoi cells whose points drift on
// sine paths; the F2 - F1 edge distance gives the bright web. Rendered once per frame into a
// small render texture and projected top-down onto the floor, rocks and fish via the
// Lighting.hlsli light-map slot.

cbuffer Caustics : register(b0)
{
    float4 gParams;   // x = time, y = intensity
};

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float2 Hash2(float2 p)
{
    p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
    return frac(sin(p) * 43758.5453);
}

// Edge distance of a Voronoi tiling with n x n cells that wraps at the texture border.
float Web(float2 uv, float n, float t, float seed)
{
    float2 p = uv * n;
    float2 ip = floor(p);
    float2 fp = frac(p);
    float f1 = 8.0, f2 = 8.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            float2 g = float2(x, y);
            float2 cell = ip + g;
            cell -= n * floor(cell / n);                       // wrap so the tile repeats seamlessly
            float2 h = Hash2(cell + seed);
            float2 o = 0.5 + 0.42 * sin(t * (0.6 + 0.8 * h.yx) + h * 6.2831);
            float d = length(g + o - fp);
            if (d < f1) { f2 = f1; f1 = d; }
            else if (d < f2) f2 = d;
        }
    }
    return f2 - f1;
}

float4 main(PSIn i) : SV_Target
{
    float t = gParams.x;
    float e1 = Web(i.uv, 5.0, t * 0.45, 0.0);
    float e2 = Web(i.uv, 8.0, t * 0.32 + 3.0, 17.0);
    float c = pow(saturate(1.0 - e1 * 2.2), 5.0) + 0.55 * pow(saturate(1.0 - e2 * 2.6), 6.0);
    c *= gParams.y;
    return float4(c, c, c, 1.0);
}
